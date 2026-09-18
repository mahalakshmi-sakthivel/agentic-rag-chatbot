# Phase 1 — C++ Backend & REST API

Foundation backend for the Plug-and-Play Agentic RAG Chatbot. Implements
`GET /v1/health`, `POST /v1/query` (stub), and `POST /v1/data/upload` per
`PHASE_1_CONTRACT.md` and `TECHNICAL_CONTRACT.md` v1.1, with the seams later
phases plug into.

**This has been compiled and run end-to-end in a clean Ubuntu 24.04
environment** — full build, 28 unit tests, and a 36-assertion live smoke
test covering real authentication enforcement. Full dated transcript in
`BUILD_VERIFICATION.md`.

> **Review-round note:** this revision addresses Team Lead's second-round
> feedback — see "Changes in this revision" below, mapped comment-by-comment.
> One item (P1→P2→P3 integration test) is flagged as blocked, not silently
> skipped — see that section for why and what's needed to unblock it.

## 1. Install dependencies

Ubuntu/Debian (apt) — fastest path, what this was actually tested with:

```bash
sudo apt-get update
sudo apt-get install -y \
  libdrogon-dev nlohmann-json3-dev libgtest-dev \
  libjsoncpp-dev libpq-dev libsqlite3-dev libmariadb-dev-compat \
  libhiredis-dev libyaml-cpp-dev cmake build-essential jq
```

The Drogon package on Ubuntu is built with optional Postgres/MySQL/SQLite/
Redis/YAML support enabled, so `find_package(Drogon)` pulls those dev
headers in transitively even though this project only uses the HTTP layer —
that's why the list is longer than just "Drogon." `jq` is only needed to run
`examples/smoke_test.sh`.

Alternative — vcpkg (matches `vcpkg.json` at the repo root; recommended if
your OS/CI isn't Ubuntu, since it resolves the same pinned dependency
versions everywhere):

```bash
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$(pwd)/vcpkg
```

`vcpkg.json` pins a `builtin-baseline` commit, so every machine that builds
against it resolves the **same** Drogon/nlohmann-json/gtest versions.

## 2. Configure environment

```bash
cp .env.example .env
# edit .env if needed — defaults work for local dev
export $(grep -v '^#' .env | xargs)   # or use your shell's .env loader
```

For local testing of the authenticated endpoints, also set:
```bash
export AUTH_DEV_BYPASS=true
```
See "Authentication" below for exactly what this does and why it's safe.

## 3. Build

```bash
cd backend
cmake --preset apt              # or: cmake --preset vcpkg   (needs VCPKG_ROOT set)
cmake --build --preset apt
```

Or the manual equivalent, if you're not using presets:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
```

Produces two binaries: `agentic_rag_backend` (the server) and
`agentic_rag_backend_tests` (GoogleTest suite).

See `BUILD_VERIFICATION.md` for a full dated transcript of this exact
sequence run from a clean environment, with exact package versions and
timings — that's what "verify the complete build" means concretely here.

### Why the CMake setup changed (previous review round)

The old CMakeLists.txt hand-maintained a flat file list, which silently
drops new files and had no way to see Phase 2/3 once they land. Fixed with
`file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` (source list generated from the
folder, not maintained separately) plus a sibling-module discovery loop
that auto-links `src/auth/`, `src/ingestion/`, etc. the moment each gets
its own `CMakeLists.txt` — verified by simulating a dummy `src/auth/`
module and confirming the root build picked it up automatically, then
removing the simulation. Added `CMakePresets.json`; pinned `vcpkg.json`'s
`builtin-baseline`.

**Open assumption, unresolved:** each sibling phase is expected to expose a
CMake target named after its folder (`src/auth/` → target `auth`). Nobody
has confirmed this with Yuvan (Phase 2) or Kavisharmitha (Phase 3) yet —
see "Blocked: P1→P2→P3 integration" below.

## 4. Run

```bash
AUTH_DEV_BYPASS=true ./build/agentic_rag_backend
```

Starts listening on `BACKEND_PORT` (default `8080`).

```bash
curl http://localhost:8080/v1/health
```

## 5. Testing

```bash
cd build
./agentic_rag_backend_tests
```

**28 unit tests**, all passing — request validation (Section 8), filename
sanitization / path-traversal rejection (Section 11), the response envelope
+ error code mapping (Sections 6 & 9), `IdentityContext`'s exact Section
9.2 JSON shape, and — new this round — the authentication enforcement
mechanism itself (`devBypassVerifier` behavior, the `setTokenVerifier`
override hook Phase 2 will use). Pure unit tests, no live server needed.

### Live endpoint + auth enforcement verification

```bash
AUTH_DEV_BYPASS=true ./build/agentic_rag_backend &
./examples/smoke_test.sh
```

**36/36 assertions passing.** New this round: the script now verifies real
authentication enforcement, not just validation/error paths — `/v1/query`
and `/v1/data/upload` correctly return `401 UNAUTHENTICATED` with no token
or the wrong token, and correctly succeed with a valid dev-bypass token,
while `/v1/health` stays open with no token at all. Full transcript in
`BUILD_VERIFICATION.md`.

## Authentication (this round's main change)

**Before:** `authenticate(req)` always returned an empty, unauthenticated
context, and nothing checked it — protected routes accepted everything.
Team Lead correctly flagged this as not actually connected.

**Now:** `common/identity.h` implements a real, swappable verification
mechanism:

- `QueryController` and `UploadController` call `common::authenticate(req)`
  and **actually reject** the request with `401 UNAUTHENTICATED` when
  `identity.authenticated` is false. This is real enforcement, not a
  comment describing future enforcement.
- The active verifier defaults to `common::devBypassVerifier` — a
  documented, non-cryptographic placeholder: a request needs
  `Authorization: Bearer dev-local-only` **and** `AUTH_DEV_BYPASS=true` in
  the environment to be treated as authenticated. Without the env flag set
  to `true`, even the correct token is rejected — verified explicitly in
  `BUILD_VERIFICATION.md` Step 6, since this is the property that makes it
  safe to leave in the codebase at all (Section 19.1: must be false outside
  local dev).
- `common::setTokenVerifier(fn)` is the actual Phase 2 integration point —
  a real function, not a hypothetical one. Once Phase 2's JWT verification
  function exists, `main.cpp` has one documented line to add:
  ```cpp
  common::setTokenVerifier(auth::verifyJwtBearerToken);
  ```
  No controller changes needed.

**What Phase 1 still does not do, and per Section 9.1 must not do: parse
or cryptographically verify a real JWT.** That's Phase 2's code to write,
not a rewrite Phase 1 should absorb into its own PR.

## Blocked: P1 → P2 → P3 integration test

Team Lead's fourth item — "after Phase 2 and Phase 3 interfaces are
connected, test the actual P1 → P2 → P3 flow" — **cannot be done from
Phase 1's side alone**, and isn't attempted in this revision. Concretely:

- There is no `src/auth/` or `src/ingestion/` code available to build or
  link against yet. The sibling-module CMake mechanism (above) is ready to
  pick both up automatically the moment they exist with their own
  `CMakeLists.txt` — but an empty folder has nothing to connect to.
- Testing "the actual flow" means running a real request through real
  Phase 2 auth and real Phase 3 ingestion — which requires their
  implementations, not simulated stand-ins written under Phase 1's PR.

**What would unblock this immediately:** Yuvan's and Kavisharmitha's branch
names/commit, so their code can be pulled in locally (not merged into this
PR — just for integration testing) and wired through
`common::setTokenVerifier` and the file-storage handoff. Once that's
available, re-running `BUILD_VERIFICATION.md`'s steps against the combined
build is straightforward — the seams on Phase 1's side are built and
tested specifically so that step is mechanical, not a redesign.

## Project layout

```
backend/
├── src/
│   ├── api/
│   │   ├── controllers/     # HealthController, QueryController, UploadController
│   │   ├── models/          # pure validation logic (framework-free, unit tested)
│   │   └── router.cpp/.h    # the entire public route surface, in one place
│   ├── common/               # Team-Lead-controlled shared contracts
│   │   ├── envelope.h       # success/error response wrapper — use this, never build raw JSON
│   │   ├── error_codes.h    # error codes Phase 1 may emit (now includes UNAUTHENTICATED — see note in that file)
│   │   ├── identity.h       # IdentityContext + real, swappable auth enforcement
│   │   ├── config.h         # env var loading (AppConfig), incl. AUTH_DEV_BYPASS
│   │   └── uuid.h           # UUID v4 generator
│   ├── auth/, ingestion/, retrieval/, orchestrator/, llm/   # NOT present yet —
│   │                          reserved per Section 23. Root CMakeLists.txt
│   │                          picks these up automatically once each has
│   │                          its own CMakeLists.txt.
│   └── main.cpp               # documents the exact Phase 2 wiring line
├── tests/api/                 # GoogleTest — validation + envelope + identity + auth enforcement
├── examples/                   # example payloads + jq-based envelope/auth smoke test
├── CMakeLists.txt
├── CMakePresets.json
├── BUILD_VERIFICATION.md       # NEW — dated, from-scratch build/test/smoke-test transcript
└── data/uploads/                # default local storage dir (gitignored)
```

## Seams for later phases (do not implement here)

| Seam | Where | Who fills it in |
|---|---|---|
| Auth | `common::setTokenVerifier()` in `common/identity.h` — one call in `main.cpp` swaps the placeholder for Phase 2's real verifier | Phase 2 |
| Query handling | `QueryController::queryHandler` (a reassignable `std::function`) | Phase 5 / Phase 6 |
| File parsing | Nothing — Phase 1 only writes raw bytes to `FILE_STORAGE_PATH`, keyed by `document_id` | Phase 3 |
| Sibling build integration | `CMakeLists.txt`'s sibling-module loop — add a `CMakeLists.txt` under `src/<your-phase>/` exposing a target named after your folder | Phase 2 / 3 / 4 / 5 / 6 |

## IdentityContext (Section 9.2 — shape is frozen)

```cpp
struct IdentityContext {
    std::string user_id;
    std::string tenant_id;
    std::vector<std::string> roles;
    std::string session_id;
    bool authenticated = false;  // internal only — NOT serialized
};
```

`to_json`/`from_json` match Section 9.2 exactly. Locked in by
`tests/api/test_identity.cpp`.

## Endpoints

| Method | Path | Auth | Behavior |
|---|---|---|---|
| GET | `/v1/health` | No | Real — status, uptime, version |
| POST | `/v1/query` | **Enforced** (401 if missing/invalid) | Stub — validates request, returns placeholder answer |
| POST | `/v1/data/upload` | **Enforced** (401 if missing/invalid) | Real — receives + stores file, returns metadata |

**No API fields or routes changed this round.** `UNAUTHENTICATED` was added
to the error code list on Team Lead's explicit instruction to connect real
enforcement — see the provenance note in `common/error_codes.h`.

## Known open items (per contract Section 31)

- **Max upload size**: defaulted to 50MB — still TBD per contract.
- **`/v1/chat` alias**: not registered; Phase 1 follows Section 7.2/8.1's `/v1/query`.
- **Sibling-module target naming assumption**: unconfirmed with Phase 2/3 — see "Blocked" above.
- **AUTH_DEV_BYPASS discipline**: must stay `false` outside local dev. `main.cpp` logs a warning if it's `true` while `ENVIRONMENT` isn't `development`, but that's a safety net, not a substitute for CI/deploy config discipline.

## Changes in this revision (response to Team Lead's second review)

| Review comment | What changed |
|---|---|
| Connect the Phase 2 auth integration point; protected routes don't enforce auth | `QueryController`/`UploadController` now actually return 401 when unauthenticated. `common/identity.h` rewritten with a real swappable verifier (`devBypassVerifier` default, `setTokenVerifier` as the one-line Phase 2 hook). `UNAUTHENTICATED` added to `error_codes.h` with a provenance note. Real JWT verification itself is explicitly left to Phase 2 — flagged, not implemented here. |
| Verify the complete CMake build in a properly configured environment, share setup steps | `BUILD_VERIFICATION.md` (new) — dated, from-scratch transcript: exact OS/compiler/package versions, every command run verbatim from this README, every result captured, including one pre-existing upstream CMake warning explicitly identified as not-ours-to-fix. |
| Run live endpoint tests for health, query, upload, error responses | `examples/smoke_test.sh` extended — 36 assertions, now including the 401-without-token / 401-wrong-token / 200-with-valid-token cases for both `/v1/query` and `/v1/data/upload`, plus a separate check that the bypass token is rejected when `AUTH_DEV_BYPASS` isn't explicitly `true`. |
| Test the actual P1→P2→P3 flow after Phase 2/3 are connected | **Blocked, not done** — see "Blocked: P1 → P2 → P3 integration test" above for exactly why and what unblocks it. |

Full verification transcript: `BUILD_VERIFICATION.md`.

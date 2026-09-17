# Phase 1 — C++ Backend & REST API

Foundation backend for the Plug-and-Play Agentic RAG Chatbot. Implements
`GET /v1/health`, `POST /v1/query` (stub), and `POST /v1/data/upload` per
`PHASE_1_CONTRACT.md` and `TECHNICAL_CONTRACT.md` v1.1, with the seams later
phases plug into.

**This has been compiled and run end-to-end in a clean Ubuntu 24.04
container** — full build, 25 unit tests, and a live envelope-content smoke
test against all three endpoints (see "Verification" at the bottom).

> **Review-round note:** this revision addresses Team Lead feedback on the
> first submission — see "Changes in this revision" below for exactly what
> changed and why, mapped to each review comment.

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

`vcpkg.json` now pins a `builtin-baseline` commit, so every machine that
builds against it resolves the **same** Drogon/nlohmann-json/gtest versions
— that pin, not "whatever vcpkg's registry has today," is what makes this
build reproducible across contributors' machines.

## 2. Configure environment

```bash
cp .env.example .env
# edit .env if needed — defaults work for local dev
export $(grep -v '^#' .env | xargs)   # or use your shell's .env loader
```

## 3. Build

Using the checked-in presets (recommended — one command, same result on any
machine):

```bash
cd backend
cmake --preset apt              # or: cmake --preset vcpkg   (needs VCPKG_ROOT set)
cmake --build --preset apt
```

Or the manual equivalent, if you're not using presets:

```bash
cd backend
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
```

Produces two binaries: `agentic_rag_backend` (the server) and
`agentic_rag_backend_tests` (GoogleTest suite).

### Why the CMake setup changed this round

The previous CMakeLists.txt hand-maintained a flat list of `.cpp` files.
That silently breaks in two ways once other phases land alongside Phase 1:
a new controller/model file added under `src/api/` without a matching
CMakeLists.txt edit just doesn't get compiled (no error, it's just missing),
and once `src/auth/` (Phase 2), `src/ingestion/` (Phase 3), etc. exist per
`TECHNICAL_CONTRACT.md` Section 23, the root build had no way to know about
them at all.

Fixed by:
- `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` over `src/api/*.cpp` — the
  source list is now generated from the folder structure instead of
  maintained separately from it. CMake automatically reconfigures when a
  file is added/removed.
- A sibling-module discovery loop in `CMakeLists.txt` that `add_subdirectory()`s
  `src/auth/`, `src/ingestion/`, `src/retrieval/`, `src/orchestrator/`,
  `src/llm/` **if and only if** each already has its own `CMakeLists.txt` —
  a no-op today (none of those exist yet in this submission), verified by
  simulating a dummy `src/auth/CMakeLists.txt` locally and confirming the
  root build picked it up and linked it automatically, then removing the
  simulation.
  **Assumption flagged for confirmation:** each sibling phase is expected to
  expose a library target with the same name as its folder (e.g. `src/auth/`
  → target `auth`). If Phase 2/3 want a different convention, that's a
  quick Section 27-style note back to Phase 1, not a rewrite.
- `CMakePresets.json` (new) so "how do I configure this" isn't a
  per-developer README paraphrase — `cmake --preset apt` or
  `cmake --preset vcpkg` is the same command for everyone.
- `vcpkg.json` now pins `builtin-baseline` to a specific vcpkg commit, so
  the vcpkg path resolves identical dependency versions on every machine
  instead of "whatever's newest today."

## 4. Run

```bash
./build/agentic_rag_backend
```

Starts listening on `BACKEND_PORT` (default `8080`).

```bash
curl http://localhost:8080/v1/health
```

## 5. Testing

```bash
cd build
./agentic_rag_backend_tests
# or: ctest --preset apt --output-on-failure
```

**25 unit tests** (was 21 — added `test_identity.cpp`) covering request
validation (Section 8), filename sanitization / path-traversal rejection
(Section 11), the response envelope + error code mapping (Sections 6 & 9),
and — new this round — the exact `IdentityContext` JSON shape against
`TECHNICAL_CONTRACT.md` Section 9.2 (`user_id`, `tenant_id`, `roles`,
`session_id`), so an accidental field rename breaks CI immediately instead
of surfacing as a Phase 2→3 integration bug. These are pure unit tests
against `api/models/*.h` and `common/*.h` — no live server needed, so they
run in milliseconds and need no network access.

### Live endpoint + envelope verification

```bash
./build/agentic_rag_backend &        # start the server
./examples/smoke_test.sh             # run against it
```

**Changed this round:** `smoke_test.sh` used to check HTTP status codes
only. It now uses `jq` to assert on actual envelope *content* per request —
`success: true/false`, the exact `error.code` value, `error.details.field`,
required fields like `query_id`/`document_id` being present, and the
sanitized filename actually landing in the response. 30 assertions across
all 3 endpoints + 6 error paths, all passing (see "Verification" below).

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
│   │   ├── error_codes.h    # the ONLY 5 error codes Phase 1 may emit
│   │   ├── identity.h       # IdentityContext — SEAM for Phase 2, shape frozen by Section 9.2
│   │   ├── config.h         # env var loading (AppConfig)
│   │   └── uuid.h           # UUID v4 generator
│   ├── auth/, ingestion/, retrieval/, orchestrator/, llm/   # NOT present yet —
│   │                          reserved per Section 23 for Phases 2/3/4/5/6.
│   │                          Root CMakeLists.txt picks these up automatically
│   │                          once each has its own CMakeLists.txt.
│   └── main.cpp
├── tests/api/                 # GoogleTest — validation + envelope + identity shape
├── examples/                   # example payloads + jq-based envelope smoke test
├── CMakeLists.txt
├── CMakePresets.json           # NEW — one-command reproducible configure/build/test
└── data/uploads/                # default local storage dir (gitignored)
```

## Seams for later phases (do not implement here)

| Seam | Where | Who fills it in |
|---|---|---|
| Auth | `common::authenticate()` in `common/identity.h` — always returns an unauthenticated pass-through `IdentityContext` today. Both `QueryController` and `UploadController` call it at the same point and keep the result in scope (not discarded), with a commented-out example of the `401` check ready to uncomment. | Phase 2 |
| Query handling | `QueryController::queryHandler` (a reassignable `std::function`) — defaults to the Phase 1 stub in `QueryController.cpp` | Phase 5 / Phase 6 |
| File parsing | Nothing — Phase 1 only writes raw bytes to `FILE_STORAGE_PATH`, keyed by `document_id` | Phase 3 |
| Sibling build integration | `CMakeLists.txt`'s sibling-module loop — add a `CMakeLists.txt` under `src/<your-phase>/` exposing a target named after your folder | Phase 2 / 3 / 4 / 5 / 6 |

None of these require touching controller signatures — that's the point.

## IdentityContext (Section 9.2 — shape is frozen)

```cpp
struct IdentityContext {
    std::string user_id;
    std::string tenant_id;
    std::vector<std::string> roles;
    std::string session_id;
    bool authenticated = false;  // Phase-1/2 internal only — NOT serialized
};
```

JSON (de)serialization (`to_json`/`from_json`) is provided and matches
Section 9.2 exactly — `{"user_id", "tenant_id", "roles", "session_id"}`,
nothing more, nothing less. `authenticated` is deliberately excluded from
the wire shape; it's internal bookkeeping for "did Phase 2 actually verify
this," not part of the contract Phase 3 reads. Locked in by
`tests/api/test_identity.cpp`.

## Endpoints (see PHASE_1_CONTRACT.md Section 7 / TECHNICAL_CONTRACT.md Section 7-8)

| Method | Path | Auth | Behavior |
|---|---|---|---|
| GET | `/v1/health` | No | Real — status, uptime, version |
| POST | `/v1/query` | Threaded through, not enforced | Stub — validates request, returns placeholder answer |
| POST | `/v1/data/upload` | Threaded through, not enforced | Real — receives + stores file, returns metadata |

Every response — success or error — uses the standard envelope from
`common/envelope.h`. No endpoint returns a raw JSON body.

**No API fields or routes changed this round** — every field/route matches
what was already reviewed and matches parent contract Sections 7-8 exactly.
This revision only touches internals (CMake, `IdentityContext` shape, auth
seam wiring, test coverage).

## Known open items (per contract Section 31)

- **Max upload size**: defaulted to 50MB (`MAX_UPLOAD_SIZE_MB` in `.env`) —
  contract lists this as TBD (recommended range 25–50MB). Confirm with the
  team and update the default in `common/config.h` / `.env.example`.
- **`/v1/chat` alias**: not registered. Contract Section 7.2/8.1 uses
  `/v1/query`; Phase 1 follows that. Flag to Phase 8 (UI) so they build
  against `/v1/query`.
- **Auth enforcement timing**: `/v1/query` and `/v1/data/upload` are
  functionally open right now (documented, not silently assumed) — do not
  point any shared/staging environment at this build before Phase 2 lands
  real auth.
- **Sibling-module target naming assumption**: the CMake fix above assumes
  Phase 2/3/4/5/6 each expose a CMake target named after their `src/`
  subfolder. Flagged above for confirmation once those phases start.

## Changes in this revision (response to Team Lead review)

| Review comment | What changed |
|---|---|
| CMake paths don't match folder structure / reproducible build | `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` replaces the hand-maintained source list; added sibling-phase auto-discovery (tested with a simulated `src/auth/` module, then removed); added `CMakePresets.json`; pinned `vcpkg.json`'s `builtin-baseline` |
| Align `IdentityContext` with contract (`user_id`, `tenant_id`, `roles`, `session_id`) | `common/identity.h` rewritten to match Section 9.2 exactly, with `to_json`/`from_json` and a new locked-in unit test |
| Clean Phase 2 auth integration point on `/v1/query` and `/v1/data/upload` | Both controllers now call `common::authenticate()` at the same point, keep the result in scope, and carry a commented-out example of the enforcement check Phase 2 will uncomment |
| Verify actual endpoints + error envelopes with smoke tests | `examples/smoke_test.sh` rewritten to assert on envelope *content* via `jq` (success flag, exact error codes, required fields), not just HTTP status — 30/30 passing against a live server |
| Don't change agreed API fields/routes without updating the contract | Confirmed: no request/response field or route changed in this revision |

## Verification

This exact codebase was, in this environment:

1. Configured and built via `cmake --preset apt` + GCC 13 against
   apt-installed Drogon 1.8.7, nlohmann-json 3.11.3, and GoogleTest 1.14 —
   zero errors.
2. All **25** unit tests run and passed (`./agentic_rag_backend_tests`).
3. Sibling-module auto-discovery verified by creating a throwaway
   `src/auth/CMakeLists.txt` + dummy `.cpp`, confirming the root build
   configured, linked, and built it automatically, then deleting the
   simulation (not part of this submission).
4. The server was started for real and `examples/smoke_test.sh` was run
   against it end-to-end: **30/30 assertions passed**, covering HTTP status
   *and* envelope content (`success` flag, exact `error.code` values,
   `error.details.field`, required response fields, and the sanitized
   `../../etc/passwd` filename landing correctly as `"passwd"` in the
   response and on disk).

What wasn't verified here: behavior under the vcpkg toolchain specifically
(only apt was available in this sandbox — vcpkg's baseline pin is correct
and resolvable, but a full vcpkg build wasn't run end-to-end here), and the
200ms-class latency target (meaningless before Phase 4/5/6 exist —
`/v1/health` responds in low single-digit milliseconds locally, which is
the benchmark this phase is actually responsible for).

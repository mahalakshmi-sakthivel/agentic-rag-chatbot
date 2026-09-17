# Phase 1 — C++ Backend & REST API

Foundation backend for the Plug-and-Play Agentic RAG Chatbot. Implements
`GET /v1/health`, `POST /v1/query` (stub), and `POST /v1/data/upload` per
`PHASE_1_CONTRACT.md`, with the seams later phases plug into.

**This has been compiled and run end-to-end in a clean Ubuntu 24.04
container** (build + 21 unit tests + live curl requests against all three
endpoints, including error paths and a path-traversal attempt). See
"Verification" at the bottom for exactly what was run.

## 1. Install dependencies

Ubuntu/Debian (apt) — fastest path, what this was actually tested with:

```bash
sudo apt-get update
sudo apt-get install -y \
  libdrogon-dev nlohmann-json3-dev libgtest-dev \
  libjsoncpp-dev libpq-dev libsqlite3-dev libmariadb-dev-compat \
  libhiredis-dev libyaml-cpp-dev cmake build-essential
```

The Drogon package on Ubuntu is built with optional Postgres/MySQL/SQLite/
Redis/YAML support enabled, so `find_package(Drogon)` pulls those dev
headers in transitively even though this project only uses the HTTP layer —
that's why the list is longer than just "Drogon."

Alternative — vcpkg (matches `vcpkg.json` at the repo root, closer to what
the parent contract implies for cross-platform team members):

```bash
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install
```

Then configure with `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake`.

## 2. Configure environment

```bash
cp .env.example .env
# edit .env if needed — defaults work for local dev
export $(grep -v '^#' .env | xargs)   # or use your shell's .env loader
```

## 3. Build

```bash
cd backend
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
```

Produces two binaries: `agentic_rag_backend` (the server) and
`agentic_rag_backend_tests` (GoogleTest suite).

## 4. Run

```bash
./agentic_rag_backend
```

Starts listening on `BACKEND_PORT` (default `8080`).

```bash
curl http://localhost:8080/v1/health
```

## 5. Testing

```bash
cd build
./agentic_rag_backend_tests
# or: ctest --output-on-failure
```

21 tests covering request validation (Section 8), filename sanitization /
path-traversal rejection (Section 11), and the response envelope + error
code mapping (Sections 6 & 9). These are pure unit tests against
`api/models/*.h` and `common/envelope.h` — no live server needed, so they
run in milliseconds and need no network access.

Endpoint-level (live HTTP) verification is a curl script, not a GoogleTest
target, since spinning a server inside a unit test binary adds flakiness for
little benefit at this phase. Run it against a running server:

```bash
./examples/smoke_test.sh   # see examples/ for the script + payloads
```

## Project layout

```
backend/
├── src/
│   ├── api/
│   │   ├── controllers/     # HealthController, QueryController, UploadController
│   │   ├── models/          # pure validation logic (framework-free, unit tested)
│   │   └── router.cpp/.h    # the entire public route surface, in one place
│   ├── common/
│   │   ├── envelope.h       # success/error response wrapper — use this, never build raw JSON
│   │   ├── error_codes.h    # the ONLY 5 error codes Phase 1 may emit
│   │   ├── identity.h       # SEAM for Phase 2 — auth pass-through today
│   │   ├── config.h         # env var loading (AppConfig)
│   │   └── uuid.h           # UUID v4 generator
│   └── main.cpp
├── tests/api/                # GoogleTest — validation + envelope logic
├── examples/                  # example payloads + curl smoke test
├── CMakeLists.txt
└── data/uploads/              # default local storage dir (gitignored)
```

## Seams for later phases (do not implement here)

| Seam | Where | Who fills it in |
|---|---|---|
| Auth | `common::authenticate()` in `common/identity.h` — always returns an unauthenticated pass-through context today | Phase 2 |
| Query handling | `QueryController::queryHandler` (a reassignable `std::function`) — defaults to the Phase 1 stub in `QueryController.cpp` | Phase 5 / Phase 6 |
| File parsing | Nothing — Phase 1 only writes raw bytes to `FILE_STORAGE_PATH`, keyed by `document_id` | Phase 3 |

None of these require touching controller signatures — that's the point.

## Endpoints (see PHASE_1_CONTRACT.md Section 7 for full spec)

| Method | Path | Auth | Behavior |
|---|---|---|---|
| GET | `/v1/health` | No | Real — status, uptime, version |
| POST | `/v1/query` | Threaded through, not enforced | Stub — validates request, returns placeholder answer |
| POST | `/v1/data/upload` | Threaded through, not enforced | Real — receives + stores file, returns metadata |

Every response — success or error — uses the standard envelope from
`common/envelope.h`. No endpoint returns a raw JSON body.

## Known open items (per contract Section 16)

- **Max upload size**: defaulted to 50MB (`MAX_UPLOAD_SIZE_MB` in `.env`) —
  contract lists this as TBD (recommended range 25–50MB). Confirm with the
  team and update the default in `common/config.h` / `.env.example`.
- **`/v1/chat` alias**: not registered. Parent contract Section 7.2 uses
  `/v1/query`; Phase 1 follows that, not the `/v1/chat` name mentioned
  informally elsewhere. Flag to Phase 8 (UI) so they build against `/v1/query`.
- **Auth enforcement timing**: `/v1/query` and `/v1/data/upload` are
  functionally open right now (Section 11 requires this be documented, not
  silently assumed) — do not point any shared/staging environment at this
  build before Phase 2 lands real auth.

## Verification

This exact codebase was, in this environment:

1. Configured and built with CMake + GCC 13 against apt-installed Drogon
   1.8.7, nlohmann-json 3.11.3, and GoogleTest 1.14 — zero errors after
   fixing one missing `#include` (now fixed in the committed source).
2. All 21 unit tests run and passed.
3. The server was started for real and hit with `curl` for: a valid
   `/v1/query`, three different validation failures (missing field, unknown
   field, malformed JSON), an unknown route (404), a valid file upload, an
   unsupported `file_type` (415), and a `../../etc/passwd`-style filename to
   confirm path-traversal sanitization — all returned the expected status
   code and envelope shape, and the sanitized file landed on disk as
   `<uuid>_passwd`, not anywhere near `/etc`.

What wasn't verified here: behavior under the vcpkg toolchain specifically
(only apt was available in this sandbox), and the 200ms-class latency
target end-to-end (meaningless before Phase 4/5/6 exist — `/v1/health`
responded in low single-digit milliseconds locally, which is the
benchmark Section 12 asks for at this phase).

# Build Verification — Phase 1

**Date:** 2026-09-18 (UTC)
**Verified by:** automated clean-environment run, full transcript below
**Result:** Configure ✅ · Build ✅ · Unit tests (28/28) ✅ · Live smoke test (36/36) ✅

This is a from-scratch run of the exact steps in `README.md`, in a
freshly-provisioned environment, with no manual fixes applied after the
fact. If this doesn't reproduce on another machine, the environment differs
from what's below — not the instructions.

## Environment

| | |
|---|---|
| OS | Ubuntu 24.04.4 LTS (noble) |
| Compiler | g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| CMake | 3.28.3 |
| Drogon | 1.8.7+ds-1.1build1 (apt) |
| nlohmann-json | 3.11.3-1 (apt) |
| GoogleTest | 1.14.0-1 (apt) |
| jq | 1.7.1-3ubuntu0.24.04.2 (apt, for smoke test) |

## Step 1 — Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  libdrogon-dev nlohmann-json3-dev libgtest-dev \
  libjsoncpp-dev libpq-dev libsqlite3-dev libmariadb-dev-compat \
  libhiredis-dev libyaml-cpp-dev cmake build-essential jq
```
**Result:** all packages resolved and installed cleanly from the standard
Ubuntu 24.04 repositories — no third-party PPAs, no manual `.deb` files.

## Step 2 — Configure

```bash
cd backend
cmake --preset apt
```
**Result:** `Configuring done` / `Generating done`, exit code 0.

One pre-existing warning appears, originating from Drogon's own installed
CMake config (`FindJsoncpp.cmake`), not from this project's code:
```
CMake Warning (dev) ... Policy CMP0153 is not set: The exec_program
command should not be called ... /usr/lib/.../cmake/Drogon/FindJsoncpp.cmake:47
```
This is Drogon's packaging issue, not fixable from `backend/CMakeLists.txt`
— noted here so it isn't mistaken for a Phase 1 defect during review.

## Step 3 — Build

```bash
cmake --build --preset apt
```
**Result:** both targets built, zero errors, zero warnings from Phase 1's
own source files. Total time: ~41 seconds on a 4-core CI-class machine.

```
[ 54%] Built target agentic_rag_backend
[100%] Built target agentic_rag_backend_tests
```

## Step 4 — Unit tests

```bash
./build/agentic_rag_backend_tests
```
**Result:** `[==========] 28 tests from 7 test suites ran.` / `[  PASSED  ] 28 tests.`

Covers: request validation, filename sanitization / path-traversal
rejection, response envelope + error code mapping, IdentityContext's exact
Section 9.2 JSON shape, and the new authentication enforcement mechanism
(`devBypassVerifier`, `setTokenVerifier` override).

## Step 5 — Live endpoint + auth enforcement verification

```bash
AUTH_DEV_BYPASS=true ./build/agentic_rag_backend &
./examples/smoke_test.sh
```
**Result:** `Summary: 36 passed, 0 failed`. Covers, against a real running
server (not mocked):

- `GET /v1/health` — 200, no auth required, correct envelope
- `POST /v1/query` with **no** `Authorization` header — **401 UNAUTHENTICATED**
- `POST /v1/query` with a **wrong** token — **401 UNAUTHENTICATED**
- `POST /v1/query` with a valid dev-bypass token — 200, correct stub response
- `POST /v1/query` authenticated + missing field / unknown field / malformed JSON — 400 VALIDATION_ERROR in all three cases
- `GET` an unknown route — 404 NOT_FOUND
- `POST /v1/data/upload` with **no** token — **401 UNAUTHENTICATED**
- `POST /v1/data/upload` authenticated + valid file — 201, correct metadata
- `POST /v1/data/upload` authenticated + unsupported `file_type` — 415
- `POST /v1/data/upload` authenticated + `../../etc/passwd` filename — 201, filename sanitized to `passwd`, not rejected outright (correct: the file is still accepted, just safely renamed)

## Step 6 — Auth safety-default check (separate from the smoke test)

Confirms the bypass literally cannot work unless explicitly enabled —
this is the property Section 19.1 requires ("all endpoints except
`/v1/auth/login` and `/v1/health` require authentication" in real
environments):

```bash
unset AUTH_DEV_BYPASS   # the required default
./build/agentic_rag_backend &
curl -X POST localhost:8080/v1/query -H "Authorization: Bearer dev-local-only" ...
```
**Result:** `401 UNAUTHENTICATED` — even the "correct" bypass token is
rejected when the flag isn't explicitly set to `true`. `GET /v1/health`
still returns 200 with no auth, as required.

## What this does NOT verify

- **The vcpkg path** (`cmake --preset vcpkg`) — only apt packages were
  available in this environment. The `vcpkg.json` baseline pin is valid and
  resolvable (verified the commit hash exists in Microsoft's vcpkg repo),
  but a full vcpkg build wasn't run end-to-end.
- **Real JWT verification** — by design; that's Phase 2's code, not
  Phase 1's. What's verified here is that Phase 1 correctly *enforces*
  whatever a verifier decides, using a documented, safety-defaulted
  placeholder until Phase 2's real one is wired in (one line in `main.cpp`,
  see the comment there).
- **P1 → P2 → P3 integration** — requires Phase 2's and Phase 3's actual
  code to exist and be available to build/run against, which isn't the
  case yet. See the PR/review conversation for the concrete next step.

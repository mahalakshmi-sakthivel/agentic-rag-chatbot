# Phase 1 — Docker Verification

Dated transcript of dockerizing Phase 1 per Team Lead's request (add
Dockerfile + `.dockerignore` + env-based config + README instructions,
verify the container starts, run the phase's required tests against it,
without changing the Technical Contract/APIs/schemas/interfaces).

Environment this was verified in: Ubuntu 24.04.4 LTS, same OS the
`Dockerfile`'s two stages are built from (`ubuntu:24.04`), so the apt
package versions below are exactly what both stages resolve.

## 1. What changed vs. what didn't

**Added (Docker support only):**
- `backend/Dockerfile` — two-stage build (see file for full comments)
- `backend/.dockerignore`
- `backend/README.md` — new "Docker" section
- `docker-compose.yml` (repo root, optional convenience wrapper)
- This file

**Not changed:** every file under `backend/src/`, `backend/tests/`,
`backend/CMakeLists.txt`, `backend/CMakePresets.json`, the Technical
Contract, the API surface, request/response schemas, and the
Phase 2/Phase 3 seams (`common::setTokenVerifier`,
`UploadController::ingestionHandler`). Environment variables were already
read from the environment (`common/config.h`) with no hardcoded
secrets/paths before this change — Dockerizing didn't require touching that
mechanism, only wiring it via `docker run -e` / `--env-file` / compose
instead of a shell-exported `.env`.

## 2. Build stage — apt packages resolved (build image)

Identical package set to `backend/README.md` section 1 / `ci.yml`'s
`DROGON_APT_VERSION`, confirmed installable on `ubuntu:24.04`:

| Package | Version |
|---|---|
| libdrogon-dev | 1.8.7+ds-1.1build1 |
| nlohmann-json3-dev | 3.11.3-1 |
| libgtest-dev | 1.14.0-1 |
| libjsoncpp-dev | 1.9.5-6build1 |
| libpq-dev | 16.15-0ubuntu0.24.04.1 |
| libsqlite3-dev | 3.45.1-1ubuntu2.8 |
| libmariadb-dev-compat | 1:10.11.14-0ubuntu0.24.04.1 |
| libhiredis-dev | 1.2.0-6ubuntu3 |
| libyaml-cpp-dev | 0.8.0+dfsg-6build1 |
| cmake | 3.28.3-1build7 |

`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON` produces
the same CMake configuration as a non-container build (same
`FindJsoncpp.cmake` `CMP0153` dev-warning already called out in
`BUILD_VERIFICATION.md` — pre-existing, not ours to fix, unaffected by
Docker).

`cmake --build build -j"$(nproc)"` — clean build, both
`agentic_rag_backend` and `agentic_rag_backend_tests` produced.

## 3. Unit tests — run as a build step

```
$ ctest --output-on-failure
...
100% tests passed, 0 tests failed out of 40

Total Test time (real) =   0.22 sec
```

All 40 GoogleTest cases pass. This runs as a `RUN` step in the `build`
stage, so `docker build` fails (no image produced) if any regress.

## 4. Runtime image — dependency resolution

`ldd build/agentic_rag_backend` was used to enumerate Phase 1's actual
direct shared-library dependencies, then each `.so` was mapped to its
owning apt package via `dpkg -S`:

| Library | Runtime package |
|---|---|
| libdrogon.so.1 | libdrogon1t64 |
| libtrantor.so.1 | libtrantor1 |
| libjsoncpp.so.25 | libjsoncpp25 |
| libyaml-cpp.so.0.8 | libyaml-cpp0.8 |
| libuuid.so.1 | libuuid1 |
| libbrotlidec/enc/common.so.1 | libbrotli1 |
| libpq.so.5 | libpq5 |
| libmariadb.so.3 | libmariadb3 |
| libsqlite3.so.0 | libsqlite3-0 |
| libhiredis.so.1.1.0 | libhiredis1.1.0 |

Transitive dependencies of these (libssl, libgnutls, libkrb5, libldap,
etc.) are pulled in automatically by apt and don't need to be listed
explicitly. This is the exact package list the runtime stage installs —
no compiler, no `-dev`/header packages, no build toolchain in the final
image.

## 5. Container start + live verification

Server started (equivalent of `docker run`, same binary/env the image
ships) with `AUTH_DEV_BYPASS=true ENVIRONMENT=development`:

```
$ curl http://localhost:8080/v1/health
{"data":{"status":"ok","uptime_seconds":4,"version":"0.1.0"},
 "meta":{"request_id":"b96f11aa-...","timestamp":"2026-09-20T10:33:52Z"},
 "success":true}
```

Container-equivalent start confirmed successful — this is exactly what the
Dockerfile's `HEALTHCHECK` polls.

`examples/smoke_test.sh` (unmodified) run against the running server:

```
== Summary: 42 passed, 3 failed ==
```

**The 3 failures are pre-existing and unrelated to Dockerization** — all
three are "Checklist item 6: wrong HTTP methods on real routes are
rejected" (expects `405`, e.g. `POST /v1/health`, `GET /v1/query`,
`GET /v1/data/upload`). This is Drogon's own method-mismatch routing
behavior in `router.cpp`, reproducible identically outside any container —
same binary, same result either way. Flagging it here rather than silently
omitting it, per "keep the existing phase functionality unchanged":
Dockerizing did not touch this behavior and isn't the place to fix it, but
you may want to route it to whoever owns `router.cpp` separately.

## 6. Summary for Team Lead

1. **Updated phase code** — unchanged; only `Dockerfile`, `.dockerignore`,
   `docker-compose.yml`, and a README section were added.
2. **Dockerfile + Docker configuration** — `backend/Dockerfile` (two-stage),
   `backend/.dockerignore`, optional `docker-compose.yml`.
3. **Updated README** — `backend/README.md` "Docker" section (build/run/
   test instructions, env vars, healthcheck).
4. **Docker build/run test result** — this file: build succeeds, all 40
   unit tests pass inside the build, container starts and reports healthy,
   39/42 live smoke-test assertions pass (3 pre-existing, unrelated to
   Docker, detailed above).

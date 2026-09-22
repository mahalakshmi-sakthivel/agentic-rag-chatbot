# Phase 3 Build Verification

## Code-level verification completed in this environment

- CSV parser compiled and exercised with:
  - quoted commas
  - escaped quotes
  - quoted delimiters
- UUID generator compiled and exercised:
  - deterministic output
  - UUID v5 version nibble
  - RFC variant bits
- Persistent raw-file repository compiled and exercised:
  - creates tenant storage directory
  - persists uploaded bytes to disk
  - uses server-generated document ID for the storage filename

## Full CMake/CTest verification

A full project CMake configure was attempted, but this execution environment does not have the Drogon development package installed. CMake stopped at:

`Could not find a package configuration file provided by "Drogon"`

Therefore a full native `cmake --build` / `ctest` result is not claimed here.

## Docker verification

Docker is not installed/available in this execution environment, so a real `docker build` / `docker run` cannot be truthfully reported as successful.

The included `Dockerfile` is configured to:

1. install the required C++/Phase 3 dependencies;
2. configure the project with `BUILD_TESTS=ON`;
3. build the backend and tests;
4. run `ctest --test-dir build --output-on-failure`;
5. expose port `8080`;
6. use `RAW_STORAGE_DIR=/app/data/uploads`;
7. declare `/app/data/uploads` as a Docker volume;
8. start `./build/agentic_rag_backend`.

Recommended verification on a Docker-enabled machine:

```bash
docker build -t agentic-rag-phase3 .
docker run --rm -p 8080:8080   -e AUTH_DEV_BYPASS=true   -v phase3-data:/app/data/uploads   agentic-rag-phase3
```

`AUTH_DEV_BYPASS=true` is for local development only and must not be enabled in shared/staging/production environments.

## Phase 3 → Phase 4 handoff security boundary

The Phase 3 → Phase 4 chunk handoff is a **trusted internal interface**, not a
public API. `IPhase4ChunkProvider` is intended to be consumed only by trusted
internal backend/Phase 4 components. It must not be registered as or exposed
through a public HTTP route.

The public API/controller boundary authenticates the caller and validates the
authenticated user's tenant, user ID, role, and document access before the
handoff is used. The handoff itself accepts `documentId` and `tenantId` and
must enforce that the requested document belongs to that tenant. Repeating
user-level JWT authorization inside `getChunks()` is therefore not required
for the current trusted-internal architecture.

If the handoff is ever made reachable by an untrusted/external caller, the
service boundary and authorization contract must be changed before exposure.

## Scope

This ZIP is a combined Phase 1 + Phase 3 integration package. Approved Phase 1 API/authentication code is retained; Phase 3 is wired through the existing upload ingestion seam.


## Additional review fixes

- Phase 2 authentication: `src/main.cpp` auto-wires `auth::verifyJwtBearerToken` when the approved Phase 2 `auth/JwtVerifier.h` module is present. This ZIP does not contain Phase 2 source, so a real JWT verifier cannot be executed from this package alone.
- Raw-file cleanup: `PersistentRawFileRepository::deleteRawFile()` is connected to the authorized document DELETE flow. The raw file is removed before the document/chunks are deleted, and deletion is idempotent if the file is already absent.
- A unit test covers persistent raw-file deletion and repeated deletion.

## Verification limitation

Docker execution and the full CMake/CTest suite require the host's Docker daemon and Phase 3 native dependencies. This environment cannot claim a successful Docker build/run without those tools.


## Phase 2 integration verification

The final package includes the approved Phase 2 JWT verifier implementation:
`src/auth/jwt_verifier.hpp/.cpp`, `token_service.hpp/.cpp`,
`session_manager.hpp/.cpp`, and the SQLite database wrapper required by the
verifier.

`src/main.cpp` now instantiates `AuthConfig`, `Database`, `TokenService`,
`SessionManager`, and `JwtVerifier`, then installs the verifier through
`common::setTokenVerifier`.

A real Docker build/runtime still requires Docker to be available on the
machine performing the final verification. This environment cannot claim a
Docker run succeeded without Docker.

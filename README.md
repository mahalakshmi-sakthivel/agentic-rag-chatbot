# Agentic RAG Backend — Phase 1 + Phase 3 Integration Package

This ZIP is a **combined Phase 1 + Phase 3 integration package**. It preserves the
approved Phase 1 API/auth code and adds the Phase 3 ingestion module under
`src/ingestion/`. It is **not** a standalone Phase 3-only replacement for Phase 1.

## What is included

- Phase 1 C++ REST API under `src/api/`
- Shared Phase 1 contracts under `src/common/`
- Phase 3 ingestion under `src/ingestion/`
  - PDF, CSV, XLSX and JSON parsers
  - validation, cleaning and chunking
  - deterministic chunk IDs
  - document/chunk repositories
  - ingestion status lifecycle
  - Phase 3 → Phase 4 chunk handoff interface
- API data routes:
  - `GET /v1/health`
  - `POST /v1/query`
  - `POST /v1/data/upload`
  - `GET /v1/data/{document_id}/status`
  - `GET /v1/data`
  - `DELETE /v1/data/{document_id}`

## Project layout

```text
.
├── src/
│   ├── api/                    # Phase 1 REST API/controllers/routes
│   ├── common/                 # Shared contracts/utilities
│   ├── ingestion/              # Phase 3 implementation
│   │   ├── chunking/
│   │   ├── cleaning/
│   │   ├── handoff/            # Phase 3 → Phase 4 boundary
│   │   ├── metadata/
│   │   ├── models/
│   │   ├── parsers/
│   │   ├── repository/
│   │   ├── status/
│   │   └── validation/
│   └── main.cpp
├── tests/
│   ├── api/
│   └── ingestion/
├── data/uploads/
├── examples/
├── Dockerfile
├── .dockerignore
├── CMakeLists.txt
└── BUILD_VERIFICATION.md
```

There is **no `backend/` directory** in this package. Run commands from the
directory containing this README.

## Build on Ubuntu 24.04

Install dependencies:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config jq \
  libdrogon-dev nlohmann-json3-dev libgtest-dev \
  libjsoncpp-dev libpq-dev libsqlite3-dev libmariadb-dev-compat \
  libhiredis-dev libyaml-cpp-dev libzip-dev libxml2-dev \
  libpoppler-cpp-dev libssl-dev
```

Configure and build:

```bash
cmake -S . -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Run unit tests:

```bash
ctest --test-dir build --output-on-failure
```

or:

```bash
./build/agentic_rag_backend_tests
```

## Run the integrated API

For local development only:

```bash
export AUTH_DEV_BYPASS=true
./build/agentic_rag_backend
```

isharmitha@DESKTOP-S3QM5CF:~/phase3-github$The Phase 1 upload handler is wired to the real Phase 3 handler in
`src/main.cpp`:

```cpp
api::UploadController::ingestionHandler = ingestion::handleIngestion;
```

Therefore uploads use the Phase 3 ingestion pipeline rather than the old
Phase 1 placeholder.

## Phase 3 → Phase 4 handoff

The handoff boundary is:

```text
src/ingestion/handoff/Phase4ChunkHandoff.h
```

It defines:

```cpp
class IPhase4ChunkProvider
{
public:
    virtual ~IPhase4ChunkProvider() = default;

    virtual std::vector<models::Chunk> getChunks(
        const std::string &documentId,
        const std::string &tenantId) = 0;
};
```

The exact Phase 3 `Chunk` schema is:

```cpp
struct Chunk
{
    std::string chunkId;
    std::string documentId;
    std::string tenantId;
    int chunkIndex = 0;
    std::string text;
    std::string sourceLocation;
    nlohmann::json metadata = nlohmann::json::object();
};
```

`DocumentRepository` implements this interface, so Phase 4 can depend on
`IPhase4ChunkProvider` rather than `InMemoryDocumentRepository`.

### Handoff access boundary and authorization

The Phase 3 → Phase 4 chunk handoff is a **trusted internal interface**. It is
not a public REST API and must not be exposed directly to external clients.
Only trusted internal Phase 4/backend components may obtain the provider and
invoke `getChunks()`.

The public document API performs the user-facing authentication and
authorization boundary before document/chunk access. At that boundary, the
authenticated identity's tenant, user ID, and role are checked against the
document ownership/access rules. The handoff additionally requires the
`documentId` and `tenantId` boundary to match the stored document, so chunks
cannot be handed across tenants or for a different document.

Because the handoff is internal and is reached only after the public API
authorization boundary, `IPhase4ChunkProvider::getChunks()` does not repeat
user-level JWT authorization. If the integration architecture changes so that
Phase 4 can call this interface from an untrusted or externally reachable
context, the interface must be protected behind an authenticated service
boundary and its authorization contract must be revised before deployment.

**Security requirement:** do not register `IPhase4ChunkProvider` or
`getChunks()` as a public HTTP route, and do not expose the repository directly
to external clients.

## Metadata flow

Upload metadata is preserved end-to-end:

```text
multipart "metadata"
        ↓
api::IngestionRequest::metadata
        ↓
models::Document::metadata
        ↓
DocumentRepository::saveDocument()
```

Each output `Chunk.metadata` contains:

```json
{
  "filename": "<sanitized filename>",
  "uploaded_by": "<authenticated user id>"
}
```

## Document status lifecycle

Phase 3 persists the contract-visible lifecycle:

```text
queued → processing → ready
```

or, when processing fails:

```text
queued → processing → failed
```

`Document.status` is persisted to the repository at each transition. Failed
documents also persist `Document.errorCode`.

`Document.uploadedAt` is populated when the document is created using the
shared UTC ISO-8601 formatter, producing values such as:

```text
2026-09-21T06:49:00Z
```

## Chunking choice

The current Phase 3 implementation uses:

- **chunk size: 4000 characters**
- **overlap: 800 characters**

This is explicitly declared in `src/ingestion/IngestionHandler.cpp` and is
covered by the Phase 3 implementation/tests. If the team contract later
changes these values, update the constants and the corresponding contract
documentation together.

## Docker

Build:

```bash
docker build -t agentic-rag-phase1-phase3 .
```

Run:

```bash
docker run --rm -p 8080:8080 \
  -e AUTH_DEV_BYPASS=true \
  agentic-rag-phase1-phase3
```

The Dockerfile performs the CMake build and runs `ctest` during image build.

## Verification

`BUILD_VERIFICATION.md` describes the checks that should be run in an
environment with Docker available. Do not treat a source checkout or ZIP
inspection as proof that Docker executed successfully.

## Temporary files

Editor swap files are excluded from the package. In particular:

```text
src/api/.router.cpp.swp
```

is intentionally absent.


## Phase 3 package scope

This ZIP is a **combined Phase 1 + Phase 3 integration package**. Phase 1 API/authentication code is retained because the Phase 3 upload seam is wired through `UploadController::ingestionHandler`. This package is not intended to replace approved Phase 1 code with a standalone Phase 3 implementation.

## Phase 3 authorization

Protected document operations enforce both tenant isolation and user-level authorization:

- A non-admin authenticated user may `GET`/inspect and `DELETE` only documents whose `ownerUserId` matches the authenticated `user_id`.
- An authenticated user may list only their own documents within their tenant.
- A user with the `admin` role may access/list/delete documents within their tenant.
- A document belonging to another user in the same tenant returns `403 PERMISSION_DENIED`.
- A document in another tenant is treated as not found.

The authorization decision uses the authenticated identity supplied by the Phase 2 authentication seam; Phase 3 does not parse JWTs itself.

## Authentication integration

When the approved Phase 2 module provides `auth/JwtVerifier.h` and
`auth::verifyJwtBearerToken`, `src/main.cpp` automatically wires that real JWT
verifier at startup. This ZIP does not contain the Phase 2 source module, so
its verifier cannot be executed from this package alone. Until the Phase 2
module is present, the development verifier is accepted only when
`AUTH_DEV_BYPASS=true`; it must not be enabled in shared, staging, or
production environments.

## Raw file storage

Raw uploads are persisted to disk through `PersistentRawFileRepository`, while retaining the `RawFileRepository` abstraction. Authorized document deletion removes the corresponding `.raw` file before deleting the logical document and chunks. Raw-file deletion is idempotent when the file is already absent. The storage root is configured with `RAW_STORAGE_DIR` and defaults to `data/uploads`. Files are stored under a tenant directory using the server-generated document ID as the filename, so the client filename is never used as a filesystem path.

For Docker, `RAW_STORAGE_DIR=/app/data/uploads` and `/app/data/uploads` is declared as a volume. Mount a persistent host volume if raw files must survive container recreation.

## CSV parsing

The CSV parser supports RFC 4180-style quoted fields, including:

- delimiters inside quoted values, e.g. `"Item with, comma"`
- escaped quotes represented as `""`
- CRLF and LF line endings
- newlines inside quoted fields

Malformed/unterminated quoted fields return `PARSE_FAILURE`.

## Chunk IDs

Chunk IDs are standards-compliant UUID v5 values. The generator uses a fixed namespace UUID and the deterministic name `documentId:chunkIndex`, applies SHA-1 as required by UUID v5, and sets the RFC variant/version bits. This makes the same document/chunk pair deterministic across restarts.

## Chunking contract

Phase 3 uses **4000 characters per chunk with 800 characters overlap**, as the agreed implementation choice. These values are explicit in `IngestionHandler.cpp` and are covered by the implementation documentation.

## Docker verification

The Dockerfile installs all Phase 3 build dependencies, builds the project with tests enabled, and runs `ctest --test-dir build --output-on-failure` during the image build. The runtime image exposes port `8080` and starts `./build/agentic_rag_backend`.

Run locally:

```bash
docker build -t agentic-rag-phase3 .
docker run --rm -p 8080:8080 -e AUTH_DEV_BYPASS=true -v phase3-data:/app/data/uploads agentic-rag-phase3
```

Then exercise the authenticated upload endpoint using the Phase 1/Phase 2 authentication mechanism. `AUTH_DEV_BYPASS` is for local development only and must not be enabled in shared/staging/production environments.


## Phase 2 Authentication Integration

This package is a combined Phase 1 + Phase 2 + Phase 3 integration package.

The approved Phase 2 JWT verifier is wired from `src/main.cpp` to the Phase 1
`common::setTokenVerifier` seam. The verifier validates JWT signature/expiry/
issuer/audience/required claims and checks the server-side session before
returning the authenticated identity used by Phase 3 authorization.

For shared/staging/production startup, set:

- `JWT_SECRET` (at least 32 characters)
- `JWT_ISSUER`
- `JWT_AUDIENCE`
- `DB_PATH`

The session migrations are under `db/migrations/` and are applied at startup
when the migration directory is available.

`AUTH_DEV_BYPASS=true` is accepted only when `ENVIRONMENT=development`.
Outside local development, startup refuses to run without the real Phase 2
JWT configuration.

## Docker

The Dockerfile installs the native Phase 3 parsing dependencies plus SQLite,
OpenSSL, and Git for the pinned `jwt-cpp` FetchContent dependency. The image
build compiles the application and runs `ctest`.

# Phase 2 — Authentication, Authorization & Session Management

## Integration Guide for Downstream Phases

---

## What Phase 2 Provides

Phase 2 is the **security gate** for the chatbot. Every protected request passes through it and receives a validated `IdentityContext`.

```
Request
  ↓
Phase 1 (Crow REST API)
  ↓
AuthenticationMiddleware   ← Phase 2
  ↓  (401 on failure)
AuthorizationMiddleware    ← Phase 2
  ↓  (403 on failure)
IdentityContext            ← Phase 2 output
  ↓
Phase 3 / 4 / 5 handlers
```

---

## IdentityContext — What Downstream Phases Receive

```cpp
#include "auth/identity_context.hpp"

// The context delivered to protected handlers:
struct IdentityContext {
    std::string              user_id;    // Verified user UUID
    std::string              tenant_id;  // Tenant/app scope
    std::vector<std::string> roles;      // ["user"] or ["admin"]
    std::string              session_id; // Non-secret session ID
};
```

**Rules for downstream phases:**
- ✅ Use `identity.user_id` as the owner of any resource
- ✅ Use `identity.tenant_id` for data isolation filters
- ✅ Use `identity.roles` to check permissions via `Authorization::has_permission()`
- ❌ Never accept `user_id` from the request body as trusted
- ❌ Never modify `user_id`, `tenant_id`, or `roles` in downstream code
- ❌ Never process the raw JWT token — only the `IdentityContext`

---

## How to Protect a Route (Phase 1)

```cpp
#include "middleware/authentication_middleware.hpp"
#include "middleware/authorization_middleware.hpp"
#include "auth/roles.hpp"

// Inject middleware into your handler:
CROW_ROUTE(app, "/files/upload").methods(crow::HTTPMethod::POST)
([&auth_middleware](crow::request& req, crow::response& res) {

    // 1. Authenticate (returns 401 if not authenticated)
    if (!auth_middleware.authenticate(req, res)) {
        res.end();
        return;
    }

    // 2. Optional: check a specific permission (returns 403 if denied)
    if (!AuthorizationMiddleware::require_permission(
            req, res, permissions::UPLOAD_DOCUMENTS)) {
        res.end();
        return;
    }

    // 3. Extract the verified identity — do NOT trust req.body["user_id"]
    std::string user_id    = req.get_header_value("X-Identity-User-Id");
    std::string tenant_id  = req.get_header_value("X-Identity-Tenant-Id");

    // 4. Pass user_id to Phase 3 for file ownership
    phase3::ingest_file(req.body, user_id, tenant_id);
    // ...
});
```

---

## Phase 2 → Phase 3 Integration (File Ingestion)

Phase 3 receives `user_id` and `tenant_id` from the `IdentityContext`, **never** from the request body.

```cpp
// Phase 3 handler receives these from middleware headers:
std::string owner_user_id  = identity.user_id;
std::string owner_tenant   = identity.tenant_id;

// Store file with correct ownership
db.execute(
    "INSERT INTO files (id, owner_user_id, tenant_id, ...) VALUES (?, ?, ?, ...)",
    {file_id, owner_user_id, owner_tenant, ...}
);
```

---

## Phase 2 → Phase 4 Integration (Retrieval)

Phase 4 must scope all retrievals to the authenticated user's scope:

```cpp
// CORRECT: scope retrieval to authenticated user
db.query(
    "SELECT * FROM documents WHERE user_id = ? AND tenant_id = ?",
    {identity.user_id, identity.tenant_id}
);

// WRONG: retrieve all documents (access control bypass)
// db.query("SELECT * FROM documents");
```

---

## Phase 2 → Phase 5 Integration (Agent)

The agent receives an immutable `IdentityContext`. It must:

- Pass `user_id` and `tenant_id` to every tool call
- Never attempt to change or ignore the identity scope
- Never call Phase 4 retrieval without the identity context

```cpp
// Correct agent tool invocation pattern:
retrieval_tool.search(query, identity.user_id, identity.tenant_id);

// WRONG — agent bypasses user scope:
// retrieval_tool.search(query, "*", "*");
```

---

## Phase 2 → Phase 8 Integration (UI)

Phase 8 (UI) calls these endpoints:

| Endpoint | Method | Auth Required |
|----------|--------|---------------|
| `/auth/register` | POST | No |
| `/auth/login` | POST | No |
| `/auth/logout` | POST | ✅ Yes |
| `/auth/me` | GET | ✅ Yes |
| `/sessions` | GET | ✅ Yes |
| `/sessions/{id}` | DELETE | ✅ Yes |

**Login response:**
```json
{
  "access_token": "<jwt>",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**Protected request header:**
```
Authorization: Bearer <access_token>
```

---

## Resource Ownership Check

Use `Authorization::can_access_resource()` before returning any user-specific resource:

```cpp
#include "auth/authorization.hpp"

ResourceContext resource;
resource.resource_id     = doc_id;
resource.owner_user_id   = doc.owner_user_id;  // from database
resource.owner_tenant_id = doc.tenant_id;       // from database
resource.resource_type   = "document";

if (!Authorization::can_access_resource(identity, resource)) {
    return crow::response(403, "Forbidden");
}
// Safe to return the document
```

---

## Building

```bash
# Prerequisites: cmake, OpenSSL, SQLite3, C++17 compiler

cd backend
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4

# Run migrations and start server
export JWT_SECRET="$(openssl rand -hex 32)"
export JWT_ISSUER="agentic-rag-chatbot"
export JWT_AUDIENCE="chatbot-api"
export DB_PATH="./data/chatbot.db"
./bin/chatbot_server
```

## Running Tests

```bash
cd backend/build
ctest --output-on-failure
```

---

## Security Checklist for New Developers

- [ ] Never log passwords, tokens, or signing secrets
- [ ] Never accept `user_id` from request body as trusted identity
- [ ] Always call `authenticate()` middleware before accessing protected resources
- [ ] Always scope database queries to `identity.user_id` and `identity.tenant_id`
- [ ] Always use `Authorization::can_access_resource()` before returning user data
- [ ] Never store password hashes — use `PasswordHasher::hash()` on registration
- [ ] Never compare passwords directly — use `PasswordHasher::verify()`
- [ ] Report any attempt to bypass authentication to Phase 7 (Security Testing)

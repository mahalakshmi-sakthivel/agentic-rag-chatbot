# PHASE 2 — AUTHENTICATION, AUTHORIZATION & SESSION MANAGEMENT

**Project:** Plug-and-Play Agentic RAG Chatbot  
**Phase Owner:** Phase 2 Developer  
**Parent Contract:** `TECHNICAL_CONTRACT.md`  
**Branch:** `phase-2-authentication-authorization`  
**Version:** 1.0

---

# 1. Introduction

Phase 2 is the **identity, access-control and session-management layer** of the chatbot.

The purpose of this phase is to make sure that every protected request can be associated with the correct user and that users can access only the resources they are permitted to access.

The basic security flow is:

```text
User
  ↓
Authentication
  ↓
Identity
  ↓
Authorization / RBAC
  ↓
Resource Ownership Check
  ↓
Protected API / Data
```

The chatbot must never rely on the frontend to decide whether a user is allowed to access a resource.

### Core rule

> **Phase 2 establishes WHO the user is, WHAT the user is allowed to access, and HOW the user's authenticated state is maintained across requests.**

Phase 2 must not become the file-ingestion layer, RAG retrieval engine, agent, LLM layer, or UI.

---

# 2. Objectives

Phase 2 must provide:

- User authentication
- Secure credential handling
- Password hashing
- Login and logout
- Access-token/session handling
- Token/session validation
- User identity extraction
- Role-Based Access Control (RBAC)
- Resource ownership checks
- User/session isolation
- Tenant/application scope preservation where applicable
- Authentication middleware
- Authorization middleware
- Security-focused API responses
- Authentication and authorization tests
- Integration interfaces for Phases 1, 3, 4, 5, 6 and 8
- Documentation for other phases

The final output of Phase 2 is an **authenticated and authorized request context** that downstream modules can safely use.

---

# 3. Position in the Overall System

```text
Phase 8 UI
   ↓
Phase 1 C++ REST API
   ↓
Phase 2 Authentication / Identity
   ├── Authentication
   ├── Token / Session Validation
   ├── User Identity
   └── Authorization / RBAC
   ↓
Phase 3 Data Ingestion
   ↓
Phase 4 RAG / Retrieval
   ↓
Phase 5 Agentic Orchestrator
   ↓
Phase 6 LLM + Response Validation
   ↓
Phase 1 Response Layer
   ↓
Phase 8 UI
```

### Phase ownership

| Responsibility | Owner |
|---|---|
| REST endpoint / HTTP server | Phase 1 |
| Authentication / Identity | **Phase 2** |
| Authorization / RBAC | **Phase 2** |
| Session / token management | **Phase 2** |
| File parsing / ingestion | Phase 3 |
| Vector / structured retrieval | Phase 4 |
| Agent planning / orchestration | Phase 5 |
| Final LLM generation + validation | Phase 6 |
| Security / performance testing | Phase 7 |
| UI / widget | Phase 8 |
| Final integration / deployment | Phase 9 |

---

# 4. What Phase 2 Must NOT Do

Phase 2 must not:

- Build the main C++ REST server from scratch if that belongs to Phase 1
- Parse uploaded PDFs, CSVs, Excel or JSON files
- Generate embeddings
- Directly implement the RAG retrieval engine
- Directly implement the Agentic RAG planner
- Generate the final LLM answer
- Decide what information the LLM should answer from
- Implement the chatbot UI
- Bypass Phase 4's retrieval filters
- Trust a `user_id` supplied by the frontend
- Give users permissions they do not possess
- Store passwords in plain text
- Log passwords, access tokens or secrets
- Accept unsigned or invalid authentication tokens
- Provide direct database access to the frontend

Phase 2 owns **identity and access control**, not every security-related feature in the system.

---

# 5. Working Principles

## 5.1 Backend Is the Security Boundary

The frontend is considered untrusted.

A client may send:

```json
{
  "user_id": "user-B"
}
```

but Phase 2 must not treat this as proof that the request belongs to `user-B`.

The authenticated identity must come from validated authentication information.

---

## 5.2 Authenticate Before Authorize

The correct sequence is:

```text
Request
  ↓
Authentication
  ↓
Identify User
  ↓
Authorization
  ↓
Resource Ownership
  ↓
Business Logic
```

A request that cannot establish a valid identity must not proceed to protected resources.

---

## 5.3 Least Privilege

Users and services should receive only the permissions required for their work.

Example:

```text
Normal User
 ├── Own sessions
 ├── Own conversations
 └── Own documents

Admin
 ├── User management
 └── Approved administrative operations
```

The exact roles and permissions must follow the project's master contract.

---

## 5.4 Fail Closed

If authentication or authorization information is missing, invalid, expired or ambiguous:

```text
DO NOT ALLOW ACCESS
```

Do not silently fall back to unrestricted access.

---

## 5.5 Never Trust Client-Supplied Ownership

Bad:

```text
POST /chat
{
    "user_id": "123"
}
```

and then using `123` as the authenticated identity.

Good:

```text
Request
 ↓
Validate Token / Session
 ↓
Extract user_id
 ↓
Create IdentityContext
 ↓
Use authenticated user_id
```

---

# 6. Authentication Architecture

A conceptual authentication architecture:

```text
                    ┌──────────────────┐
                    │    Chatbot UI    │
                    └────────┬─────────┘
                             │
                             │ Login
                             ▼
                    ┌──────────────────┐
                    │  C++ REST API    │
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │ Auth Controller  │
                    └────────┬─────────┘
                             │
                  ┌──────────▼──────────┐
                  │ Credential Service  │
                  └──────────┬──────────┘
                             │
                  ┌──────────▼──────────┐
                  │ Password Hash       │
                  │ Verification        │
                  └──────────┬──────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │ Session / Token      │
                  │ Service             │
                  └──────────┬──────────┘
                             │
                             ▼
                         Client
```

For subsequent protected requests:

```text
Client
  ↓
Authorization: Bearer <token>
  ↓
C++ REST API
  ↓
Authentication Middleware
  ↓
Token / Session Validation
  ↓
IdentityContext
  ↓
Authorization Middleware
  ↓
Protected Handler
```

---

# 7. Authentication Lifecycle

## 7.1 Registration

If registration is required by the application:

```text
User
 ↓
POST /auth/register
 ↓
Validate input
 ↓
Check whether account already exists
 ↓
Hash password
 ↓
Store user
 ↓
Return registration result
```

Passwords must never be stored directly.

Conceptually:

```text
password
   ↓
Password Hashing Algorithm
   ↓
password_hash
   ↓
Database
```

---

## 7.2 Login

```text
Client
 ↓
POST /auth/login
 ↓
Validate request
 ↓
Find user
 ↓
Verify password against stored hash
 ↓
Create authenticated session/token
 ↓
Return authentication result
```

Example conceptual response:

```json
{
  "access_token": "<token>",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

The actual response schema must follow `TECHNICAL_CONTRACT.md`.

---

## 7.3 Protected Request

```text
Client
  │
  │ Authorization: Bearer <token>
  ▼
C++ REST API
  │
  ▼
Authentication Middleware
  │
  ├── Missing → 401
  ├── Invalid → 401
  └── Expired → 401
  │
  ▼
IdentityContext
  │
  ▼
Authorization / RBAC
  │
  ├── Denied → 403
  │
  ▼
Resource Ownership
  │
  ├── Not allowed → 403 / 404
  │
  ▼
Protected Operation
```

---

# 8. Identity Context

After successful authentication, downstream modules should receive a structured identity context rather than the raw authentication token.

Example:

```json
{
  "user_id": "user-uuid",
  "tenant_id": "tenant-uuid",
  "roles": ["user"],
  "session_id": "session-uuid"
}
```

Possible C++ representation:

```cpp
struct IdentityContext {
    std::string user_id;
    std::string tenant_id;
    std::vector<std::string> roles;
    std::string session_id;
};
```

The exact schema must remain compatible with the master project contract.

### Important rule

> Only Phase 2 should process and validate the raw authentication credential/token.

Other phases should receive the resulting identity/authorization context.

---

# 9. Authentication Middleware

The authentication middleware is responsible for establishing the identity of a request.

Conceptual flow:

```text
Incoming Request
       ↓
Read Authorization Information
       ↓
Extract Credential
       ↓
Validate Credential
       ↓
Check Expiration
       ↓
Verify Signature / Integrity
       ↓
Extract Identity
       ↓
Create IdentityContext
       ↓
Continue Request
```

If validation fails:

```text
Stop Request
   ↓
Return 401
```

The middleware must not allow a protected handler to run after failed authentication.

---

# 10. Authorization & RBAC

Authentication:

> "Who are you?"

Authorization:

> "What are you allowed to do?"

RBAC can represent permissions using roles.

Example:

```text
Role
 ├── user
 └── admin
```

Example permission model:

| Operation | User | Admin |
|---|---:|---:|
| Use chatbot | ✓ | ✓ |
| Access own sessions | ✓ | ✓ |
| Access own documents | ✓ | ✓ |
| Access another user's documents | ✗ | Only if explicitly permitted |
| Manage users | ✗ | ✓ |
| Administrative operations | ✗ | ✓ |

These are examples only; the actual permission matrix should follow project requirements.

---

# 11. Authorization Flow

```text
Authenticated Request
        ↓
IdentityContext
        ↓
Required Permission
        ↓
Role / Permission Check
        ↓
Allowed?
   /          \
 YES           NO
  ↓             ↓
Continue       403
```

Authorization must be enforced server-side.

---

# 12. Resource Ownership

Authentication alone is not enough.

A user may be authenticated but still not be authorized to access a specific resource.

Example:

```text
User A
user_id = A

Document 101
owner_id = A

Document 202
owner_id = B
```

Requests:

```text
User A → Document 101 → ALLOW
User A → Document 202 → DENY
```

Conceptually:

```sql
SELECT *
FROM documents
WHERE id = ?
AND user_id = ?;
```

The ownership check should happen before protected data is returned.

---

# 13. User Data Isolation

This is a mandatory requirement because the chatbot handles user/application-specific data.

The system may contain:

```text
User
 ├── Sessions
 ├── Conversations
 ├── Uploaded Files
 ├── Documents
 ├── Retrieval Data
 └── Application Resources
```

Each user-specific resource must have a clear ownership or permission relationship.

The security chain should remain:

```text
Authenticated User
       ↓
IdentityContext
       ↓
Authorized Scope
       ↓
Data Access
```

Phase 2 must provide the identity information required by Phases 3, 4 and 5 to maintain this isolation.

---

# 14. Session Management

The application needs a mechanism to maintain an authenticated user's state between requests.

Conceptually:

```text
Login
 ↓
Session / Access Token Created
 ↓
Client uses authenticated state
 ↓
Protected API Requests
 ↓
Validation
 ↓
IdentityContext
```

A session record may conceptually contain:

```text
session_id
user_id
created_at
expires_at
status
```

The exact persistence strategy depends on the project's master architecture.

---

# 15. Token Management

If token-based authentication is used, tokens should have:

- Expiration
- User identity
- Appropriate claims
- Integrity protection
- Appropriate issuer/audience validation where applicable
- No unnecessary sensitive information

For JWT-based authentication, the backend must validate the token rather than merely decode it.

Validation should include, as applicable:

```text
Signature
Expiration
Issuer
Audience
Required Claims
Allowed Algorithm
```

Do not accept an authentication token merely because it can be decoded.

---

# 16. Logout

Logout behavior depends on the chosen authentication mechanism.

Conceptual flow:

```text
Client
 ↓
POST /auth/logout
 ↓
Validate current authentication
 ↓
Invalidate session / revoke applicable credential
 ↓
Return logout result
```

For stateless access tokens, immediate revocation may require an additional server-side mechanism if the project's requirements demand it.

The selected strategy must be documented.

---

# 17. Session Expiration

Sessions/tokens must have controlled lifetimes.

Example:

```text
Login
 ↓
Session Created
 ↓
Valid
 ↓
Expiration Time Reached
 ↓
Session Invalid
 ↓
Protected Request → 401
```

Do not allow indefinitely valid authentication credentials unless explicitly required and approved.

---

# 18. Authentication API Surface

A possible API structure is:

```text
POST /auth/register
POST /auth/login
POST /auth/logout

GET /auth/me

GET /sessions
DELETE /sessions/{session_id}
```

The exact endpoint names, request schemas and response schemas must follow the Phase 1 REST structure and `TECHNICAL_CONTRACT.md`.

### `/auth/me`

This endpoint can return the authenticated user's identity information.

Example:

```json
{
  "user_id": "user-uuid",
  "roles": ["user"]
}
```

Do not expose unnecessary sensitive account information.

---

# 19. API Status Codes

Authentication and authorization should use consistent HTTP status codes.

```text
200 → Successful request
201 → Resource created
400 → Invalid request
401 → Missing / invalid / expired authentication
403 → Authenticated but not authorized
404 → Resource not found
429 → Too many requests
500 → Internal server error
```

Do not reveal sensitive information through error messages.

---

# 20. Password Security

Passwords must never be stored as plain text.

Use a dedicated password-hashing algorithm such as an approved password hashing mechanism.

Conceptual flow:

```text
Registration:
Password
   ↓
Password Hash
   ↓
Store Hash

Login:
Password
   ↓
Verify Against Stored Hash
   ↓
Success / Failure
```

Do not:

```text
password = "mypassword123"
```

Do not store:

```text
password = "mypassword123"
```

Do not log:

```text
LOGIN password=mypassword123
```

---

# 21. Credential and Secret Handling

Never commit secrets to source control.

Do not hard-code:

```text
JWT signing secret
Database password
API key
Encryption key
Private key
```

Bad:

```cpp
const std::string SECRET = "my-super-secret";
```

Use the project's approved configuration/secret-management mechanism.

Logs must never contain:

```text
Password
Access Token
Refresh Token
Signing Secret
API Key
```

---

# 22. Brute-Force and Abuse Protection

Authentication endpoints should be designed so repeated failed login attempts cannot be abused indefinitely.

Depending on the overall architecture, protection can include:

```text
Rate limiting
 ↓
Login attempt controls
 ↓
Temporary blocking / throttling
 ↓
Security logging
```

Phase 7 owns broader security/performance testing, but Phase 2 should provide authentication behavior that can be tested and protected.

---

# 23. Security Headers / Transport

Authentication credentials must be protected during network communication.

Production deployment should use:

```text
HTTPS / TLS
```

Do not design the system assuming credentials can safely travel over plain HTTP.

Cookie-based sessions, if selected, should use appropriate security attributes such as:

```text
Secure
HttpOnly
SameSite
```

when applicable.

The exact client/session mechanism must be decided consistently with the project's architecture.

---

# 24. Integration With Phase 1

Phase 1 owns the main C++ REST API.

Phase 2 integrates authentication and authorization into that API.

Conceptual boundary:

```text
Phase 1
C++ REST Server
      ↓
Phase 2
Authentication Middleware
      ↓
Phase 2
Authorization Middleware
      ↓
Phase 1 / Other Phase Handler
```

Phase 2 should not duplicate the entire REST server implementation.

---

# 25. Integration With Phase 3

Phase 3 handles file upload and ingestion.

Phase 2 provides authenticated identity:

```text
User
 ↓
POST /files/upload
 ↓
Authentication
 ↓
user_id = U1
 ↓
Phase 3
 ↓
Store / process file for U1
```

This prevents the ingestion layer from relying on arbitrary client-supplied ownership information.

---

# 26. Integration With Phase 4

Phase 4 handles retrieval.

Phase 2 provides authorization context:

```text
User U1
 ↓
Authenticated IdentityContext
 ↓
Phase 4 Retrieval
 ↓
Authorized document scope
 ↓
Relevant chunks
```

The retrieval engine must not receive permission to access data outside the user's authorized scope.

Phase 2 establishes the identity/authorization context; Phase 4 remains responsible for applying the retrieval filters appropriate to its data store.

---

# 27. Integration With Phase 5

Phase 5 is the Agentic RAG orchestrator.

The agent must not receive or manipulate raw authentication tokens.

Instead:

```text
Phase 2
 ↓
IdentityContext
 ↓
Phase 5 Agent
 ↓
Tool Calls
 ↓
Authorized Retrieval
```

The agent must preserve:

```text
user_id
tenant_id
roles
authorized scope
```

It must not be allowed to change these values to obtain another user's information.

---

# 28. Integration With Phase 6

Phase 6 generates the final response using authorized retrieved information.

Phase 2's role is upstream:

```text
Authentication
 ↓
Authorization
 ↓
Authorized Retrieval / Agent
 ↓
Authorized Context
 ↓
Phase 6
```

Phase 2 does not decide what the LLM should say.

The LLM should never be used as the authorization mechanism.

---

# 29. Integration With Phase 7

Phase 7 performs broader security and performance testing.

Phase 2 should work with Phase 7 on:

```text
Authentication bypass
Authorization bypass
Token manipulation
Session attacks
Cross-user access
Cross-tenant access
Brute-force behavior
Malformed requests
Information leakage
Authentication latency
```

---

# 30. Integration With Phase 8

Phase 8 builds the chatbot UI.

The UI may call:

```text
POST /auth/login
POST /auth/logout
GET /auth/me
```

and then use authenticated requests for chatbot operations.

The frontend may display:

```text
Logged in
Logged out
Session expired
Unauthorized
```

but it must not be trusted to enforce backend permissions.

---

# 31. Recommended Internal Structure

A possible structure consistent with the project's C++ backend:

```text
backend/src/auth/
├── auth_controller.hpp
├── auth_controller.cpp
│
├── auth_service.hpp
├── auth_service.cpp
│
├── password_hasher.hpp
├── password_hasher.cpp
│
├── token_service.hpp
├── token_service.cpp
│
├── session_manager.hpp
├── session_manager.cpp
│
├── identity_context.hpp
├── identity_context.cpp
│
├── authorization.hpp
├── authorization.cpp
│
└── roles.hpp
    roles.cpp

backend/src/middleware/
├── authentication_middleware.hpp
├── authentication_middleware.cpp
├── authorization_middleware.hpp
└── authorization_middleware.cpp

backend/tests/auth/
├── password_tests.cpp
├── login_tests.cpp
├── token_tests.cpp
├── session_tests.cpp
├── authorization_tests.cpp
├── ownership_tests.cpp
└── integration_tests.cpp
```

The exact class/file names can differ.

The cross-phase interfaces must remain compatible with the master contract.

---

# 32. Suggested Data Model

A conceptual user model:

```text
users
-----
id
username / email
password_hash
status
created_at
updated_at
```

A conceptual session model:

```text
sessions
--------
id
user_id
created_at
expires_at
status
```

A conceptual role relationship:

```text
users
  ↓
user_roles
  ↓
roles
  ↓
permissions
```

The final schema should follow the project's selected database and master contract.

---

# 33. Authentication Service Responsibilities

The authentication service should conceptually handle:

```text
Register User
Verify Credentials
Create Authentication State
Validate Authentication State
Logout / Invalidate Session
Get Current User
```

It should not contain:

```text
File Parsing
RAG Retrieval
LLM Calls
Agent Planning
UI Logic
```

---

# 34. Authorization Service Responsibilities

The authorization layer should answer questions such as:

```text
Can this user perform this action?
Can this user access this resource?
Does this user's role contain this permission?
Does this resource belong to this user?
```

Conceptual interface:

```cpp
bool hasPermission(
    const IdentityContext& identity,
    const std::string& permission
);
```

and:

```cpp
bool canAccessResource(
    const IdentityContext& identity,
    const ResourceContext& resource
);
```

The exact interfaces may differ.

---

# 35. Authentication State

A request should have a clearly defined authenticated state.

Example:

```text
Unauthenticated
       ↓
Valid Credentials
       ↓
Authenticated
       ↓
IdentityContext
```

Protected endpoints should reject:

```text
No credentials
Invalid credentials
Expired credentials
Malformed credentials
Revoked credentials
```

where applicable to the chosen session/token design.

---

# 36. Authorization Matrix

The project should maintain a documented permission matrix.

Example:

| Resource / Action | User | Admin |
|---|---:|---:|
| Login | ✓ | ✓ |
| Logout | ✓ | ✓ |
| View own profile | ✓ | ✓ |
| Use chatbot | ✓ | ✓ |
| View own sessions | ✓ | ✓ |
| View own documents | ✓ | ✓ |
| Access another user's documents | ✗ | Only if explicitly permitted |
| Manage users | ✗ | ✓ |
| Administrative configuration | ✗ | ✓ |

Do not treat this example as the final contract without confirming the project's actual roles.

---

# 37. Cross-User Access Example

Consider:

```text
User A → user_id = A
User B → user_id = B

Document X → owner = A
Document Y → owner = B
```

User A:

```text
GET /documents/X
→ ALLOW
```

User A:

```text
GET /documents/Y
→ DENY
```

Even if User A changes the URL from:

```text
/documents/X
```

to:

```text
/documents/Y
```

the backend must still reject the unauthorized request.

This type of direct object reference test is important for Phase 7 security testing.

---

# 38. Multi-Tenant Scope

If the chatbot supports multiple applications, organizations or tenants, identity should preserve tenant scope.

Example:

```json
{
  "user_id": "U1",
  "tenant_id": "T1",
  "roles": ["user"]
}
```

The system must not silently transform:

```text
tenant_id = T1
```

into:

```text
tenant_id = T2
```

or remove the tenant filter.

The authorization context must remain intact as the request moves through Phases 3–6.

---

# 39. Error Handling

Potential internal authentication states:

```text
AUTHENTICATION_REQUIRED
INVALID_CREDENTIALS
INVALID_TOKEN
TOKEN_EXPIRED
SESSION_EXPIRED
SESSION_REVOKED
FORBIDDEN
RESOURCE_ACCESS_DENIED
USER_NOT_FOUND
ACCOUNT_DISABLED
```

Public API error codes must follow the master contract.

Avoid revealing whether a specific username/email exists if doing so would create unnecessary account-enumeration risk.

---

# 40. Logging and Observability

Authentication events should be traceable without logging secrets.

Useful operational information:

```text
request_id
user_id (where appropriate)
session_id (non-secret identifier)
event
status
timestamp
duration_ms
```

Example:

```json
{
  "request_id": "uuid",
  "event": "authentication",
  "status": "success",
  "user_id": "user-uuid",
  "duration_ms": 8
}
```

Never log:

```text
Password
Access Token
Refresh Token
JWT Secret
API Key
Private Key
```

---

# 41. Security Threats to Consider

Phase 2 should be designed against common authentication/authorization failures.

### Threats

```text
Credential theft
Token tampering
Token replay
Session fixation
Session hijacking
Brute-force login attempts
Privilege escalation
Broken access control
IDOR / object-ID manipulation
Cross-user data access
Cross-tenant data access
Account enumeration
Sensitive information leakage
```

Phase 7 performs the broader security assessment, but Phase 2 must provide secure primitives and clear boundaries for testing.

---

# 42. Testing Requirements

## 42.1 Authentication Tests

Test:

```text
Valid credentials → success
Invalid password → failure
Invalid username → failure
Missing credentials → failure
Malformed request → failure
Expired token → failure
Invalid token → failure
Tampered token → failure
```

---

## 42.2 Password Tests

Verify:

```text
Password is never stored as plain text
Password hash verification works
Wrong password is rejected
Password is not present in logs
```

---

## 42.3 Authorization Tests

Test:

```text
User with permission → allowed
User without permission → denied
Admin operation by normal user → denied
Missing role → denied
Invalid role → denied
```

---

## 42.4 Resource Ownership Tests

At minimum:

```text
User A → own resource → ALLOW
User A → User B resource → DENY
User B → User A resource → DENY
```

Also test direct identifier manipulation:

```text
/resource/A
        ↓
change ID
        ↓
/resource/B
        ↓
must still be DENIED
```

---

## 42.5 Session Tests

Test:

```text
Session creation
Session validation
Session expiration
Session invalidation
Logout
Multiple sessions
Invalid session ID
```

The exact behavior for multiple sessions should follow the selected architecture.

---

# 43. Contract Tests

Phase 2 has important boundaries with other phases.

## Phase 1 → Phase 2

Verify:

```text
HTTP Request
 ↓
Authentication Middleware
 ↓
IdentityContext
 ↓
Protected REST Handler
```

---

## Phase 2 → Phase 3

Verify:

```text
Authenticated User
 ↓
IdentityContext
 ↓
Upload / Ingestion
 ↓
Correct user association
```

---

## Phase 2 → Phase 4

Verify:

```text
IdentityContext
 ↓
Authorized Retrieval Scope
 ↓
No unauthorized document retrieval
```

---

## Phase 2 → Phase 5

Verify:

```text
IdentityContext
 ↓
Agent
 ↓
Tool Calls
 ↓
Identity/scope preserved
```

---

## Phase 2 → Phase 8

Verify:

```text
Login
 ↓
Authenticated State
 ↓
Protected REST Request
 ↓
Logout
```

---

# 44. Mocking and Independent Testing

Phase 2 should be testable without waiting for every other phase to finish.

Useful mocks:

```text
MockUserRepository
MockSessionRepository
MockTokenService
MockPasswordHasher
MockAuthorizationPolicy
```

Example:

```text
Mock User
user_id = U1
role = user
```

Then test:

```text
U1 → own document → allowed
U1 → U2 document → denied
```

This allows Phase 2 development to proceed independently.

---

# 45. Development Order

Recommended implementation order:

```text
1. Review TECHNICAL_CONTRACT.md
2. Confirm Phase 1 REST integration points
3. Define IdentityContext
4. Define user/session data model
5. Implement password hashing
6. Implement credential verification
7. Implement authentication service
8. Implement token/session service
9. Implement authentication middleware
10. Implement roles/permissions
11. Implement authorization middleware
12. Implement resource ownership checks
13. Implement auth REST endpoints
14. Add unit tests
15. Add authorization tests
16. Add cross-user isolation tests
17. Add contract tests
18. Integrate with Phase 1
19. Integrate identity context with Phase 3/4/5
20. Security review
21. Performance test
22. Prepare Pull Request
```

---

# 46. Performance Considerations

The overall project has an approximate **200 ms backend latency target under defined testing conditions**.

Phase 2 should avoid unnecessary overhead.

Important areas:

```text
Token validation
Database lookups
Session validation
Permission checks
Repeated user queries
```

Possible optimization areas:

```text
Efficient database queries
Appropriate indexing
Short-lived in-memory caching where safe
Avoiding duplicate identity lookups
Connection pooling
```

Security must not be weakened merely to reduce latency.

Phase 7 owns final performance measurement.

---

# 47. Git Rules

Use the dedicated branch:

```text
phase-2-authentication-authorization
```

Do not commit directly to `main`.

Example commits:

```text
feat(phase2): add identity context
feat(phase2): implement password hashing
feat(phase2): add authentication service
feat(phase2): add token validation middleware
feat(phase2): implement RBAC authorization
test(phase2): add cross-user access tests
fix(phase2): reject expired sessions
```

Keep commits focused and understandable.

---

# 48. Pull Request Requirements

The Phase 2 Pull Request should contain:

1. What was implemented
2. Authentication approach
3. Token/session approach
4. RBAC/authorization approach
5. User/resource isolation approach
6. API endpoints added/changed
7. Database/schema changes
8. Unit-test results
9. Authorization-test results
10. Cross-user isolation-test results
11. Security considerations
12. Performance measurements
13. Known limitations
14. Integration instructions for other phases

The PR must not introduce undocumented public API changes.

---

# 49. Definition of Done

Phase 2 is complete only when:

- [ ] Authentication architecture is implemented.
- [ ] User authentication works.
- [ ] Passwords are securely hashed.
- [ ] Plain-text passwords are never stored.
- [ ] Login works.
- [ ] Logout works according to the selected session/token strategy.
- [ ] Authentication middleware is integrated.
- [ ] Token/session validation works.
- [ ] Expired authentication is rejected.
- [ ] Invalid authentication is rejected.
- [ ] IdentityContext is created for authenticated requests.
- [ ] RBAC/authorization is implemented.
- [ ] Protected operations enforce permissions.
- [ ] Resource ownership is checked.
- [ ] Cross-user access is prevented.
- [ ] Tenant/application scope is preserved where applicable.
- [ ] Raw tokens are not passed to downstream phases unnecessarily.
- [ ] Secrets are not committed.
- [ ] Sensitive credentials are not logged.
- [ ] Authentication tests pass.
- [ ] Authorization tests pass.
- [ ] Data-isolation tests pass.
- [ ] Contract tests pass.
- [ ] Phase 1 integration works.
- [ ] Integration documentation exists.
- [ ] README/documentation exists.
- [ ] CI passes.
- [ ] Team Lead review is completed.

---

# 50. Integration Checkpoints

Do not wait until Phase 9 to discover authentication problems.

## Checkpoint 1 — Phase 1 + Phase 2

```text
REST Request
 ↓
Authentication
 ↓
IdentityContext
 ↓
Protected Endpoint
```

Verify that protected REST endpoints correctly identify the user.

---

## Checkpoint 2 — Phase 2 + Phase 3

```text
Authenticated Upload
 ↓
User Identity
 ↓
File Ingestion
 ↓
Correct Ownership
```

Verify that uploaded resources are associated with the correct user.

---

## Checkpoint 3 — Phase 2 + Phase 4

```text
Authenticated Query
 ↓
Authorized Scope
 ↓
Retrieval
 ↓
Only permitted data
```

Verify cross-user and cross-tenant isolation.

---

## Checkpoint 4 — Phase 2 + Phase 5

```text
User
 ↓
Authentication
 ↓
IdentityContext
 ↓
Agent
 ↓
Tools
```

Verify that the agent cannot change or bypass the user's authorization scope.

---

## Checkpoint 5 — Phase 8 + Backend

```text
Login
 ↓
Authenticated UI
 ↓
Chat/File APIs
 ↓
Session Expiration
 ↓
Logout
```

Verify the complete frontend authentication flow.

---

# 51. Final Security Architecture

The complete security path should be:

```text
                         USER
                           │
                           ▼
                    ┌─────────────┐
                    │  Phase 8 UI │
                    └──────┬──────┘
                           │
                           ▼
                    ┌─────────────┐
                    │ Phase 1 API │
                    └──────┬──────┘
                           │
                           ▼
                ┌──────────────────────┐
                │      PHASE 2         │
                │ Authentication       │
                │        ↓             │
                │ Identity             │
                │        ↓             │
                │ Authorization / RBAC │
                │        ↓             │
                │ Ownership Check      │
                └──────────┬───────────┘
                           │
                           ▼
                  Authorized Request
                           │
            ┌──────────────┼──────────────┐
            ▼              ▼              ▼
        Phase 3        Phase 4        Phase 5
       Ingestion      Retrieval        Agent
            │              │              │
            └──────────────┼──────────────┘
                           ▼
                       Phase 6
                    LLM + Validation
                           │
                           ▼
                       Response
```

---

# 52. Core Security Rule

The most important rule for Phase 2 is:

> **Never trust the client to tell the backend who the user is or what the user is allowed to access.**

The trusted flow is:

```text
REQUEST
   ↓
AUTHENTICATE
   ↓
IDENTIFY
   ↓
AUTHORIZE
   ↓
CHECK OWNERSHIP / SCOPE
   ↓
ALLOW PROTECTED OPERATION
```

If any required security check fails:

```text
FAIL CLOSED
   ↓
REJECT REQUEST
```

---

# 53. Success Criteria

Phase 2 is successful when the chatbot can reliably answer the following security questions for every protected request:

```text
Who is making this request?
        ↓
Is the authentication valid?
        ↓
Is the session/token valid?
        ↓
What roles/permissions does the user have?
        ↓
What tenant/application does the request belong to?
        ↓
Does the user have access to this resource?
        ↓
Can the request safely continue?
```

The final behavior should therefore be:

```text
                  Incoming Request
                         ↓
                ┌────────────────┐
                │ Authentication │
                └───────┬────────┘
                        ↓
                 Valid identity?
                    /        \
                  No          Yes
                  ↓            ↓
                 401      IdentityContext
                               ↓
                       ┌───────────────┐
                       │ Authorization │
                       └───────┬───────┘
                              ↓
                       Permission?
                         /       \
                       No         Yes
                       ↓           ↓
                      403     Ownership /
                              Scope Check
                                  ↓
                           Authorized?
                            /       \
                          No         Yes
                          ↓           ↓
                     403 / 404   Protected API
                                      ↓
                               Phase 3 / 4 / 5
                                      ↓
                                  Phase 6
```

---

# 54. Developer Quick Reference

Before requesting integration, the Phase 2 developer should be able to answer **yes** to all of these:

- [ ] Do I know exactly what Phase 2 owns?
- [ ] Do I know what Phase 1 provides?
- [ ] Do I know what Phase 3 needs from authentication?
- [ ] Do I know what Phase 4 needs for data isolation?
- [ ] Do I know what Phase 5 needs for identity/scope?
- [ ] Do I know what Phase 8 needs for login/logout?
- [ ] Are passwords securely hashed?
- [ ] Are tokens/sessions validated?
- [ ] Are expired credentials rejected?
- [ ] Is RBAC/authorization enforced server-side?
- [ ] Is resource ownership checked?
- [ ] Can User A access User B's data?
- [ ] Is tenant/application scope preserved?
- [ ] Are secrets excluded from logs and source control?
- [ ] Are authentication failures handled safely?
- [ ] Are authorization tests passing?
- [ ] Are cross-user isolation tests passing?
- [ ] Are contract tests passing?
- [ ] Does the implementation follow `TECHNICAL_CONTRACT.md`?

If any answer is **no**, resolve the issue before requesting integration.

---

# 55. Final Architecture Rule

> **Phase 2 is the security gate of the chatbot.**

Its responsibility is:

```text
AUTHENTICATE
     ↓
IDENTIFY
     ↓
AUTHORIZE
     ↓
ISOLATE
     ↓
PASS TRUSTED CONTEXT
```

It does not own:

```text
REST server              → Phase 1
File ingestion           → Phase 3
Retrieval engine         → Phase 4
Agent orchestration      → Phase 5
Final LLM generation     → Phase 6
Security/performance QA  → Phase 7
UI                       → Phase 8
Final integration        → Phase 9
```

The result of Phase 2 should be a **secure, reusable identity and authorization layer** that every downstream component can depend on.

---

**End of `PHASE_2_AUTHENTICATION_AUTHORIZATION.md`**

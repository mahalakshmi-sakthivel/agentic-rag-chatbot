# TECHNICAL_CONTRACT.md
## Plug-and-Play Agentic RAG Chatbot — Technical Contract

**Document Owner:** Team Lead
**Status:** Draft — Pending Team Sign-off
**Version:** 1.1

> ⚠️ This document is the **single source of truth** for all technical decisions on this project. Every developer must read this document before writing code for their assigned phase. Any change to this contract must go through the process defined in Section 27.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Purpose of the Technical Contract](#2-purpose-of-the-technical-contract)
3. [Scope](#3-scope)
4. [System Architecture](#4-system-architecture)
5. [Component Responsibilities](#5-component-responsibilities)
6. [Phase-wise Responsibilities](#6-phase-wise-responsibilities)
7. [API Contract](#7-api-contract)
8. [Request and Response Schemas](#8-request-and-response-schemas)
9. [Authentication & Authorization Contract](#9-authentication--authorization-contract)
10. [User/Application/Tenant Data Isolation](#10-userapplicationtenant-data-isolation)
11. [Data Ingestion Contract](#11-data-ingestion-contract)
12. [Data Schema Contract](#12-data-schema-contract)
13. [RAG & Retrieval Contract](#13-rag--retrieval-contract)
14. [Agentic RAG Contract](#14-agentic-rag-contract)
15. [Tool-Calling Contract](#15-tool-calling-contract)
16. [LLM Integration Contract](#16-llm-integration-contract)
17. [Response Validation Contract](#17-response-validation-contract)
18. [Error Handling Contract](#18-error-handling-contract)
19. [Security Requirements](#19-security-requirements)
20. [Performance & 200 ms Backend Latency Contract](#20-performance--200-ms-backend-latency-contract)
21. [Dependencies & Version Management](#21-dependencies--version-management)
22. [Configuration & Environment Variables](#22-configuration--environment-variables)
23. [Project/Folder Structure](#23-projectfolder-structure)
24. [Git & Branching Strategy](#24-git--branching-strategy)
25. [Pull Request & Code Review Rules](#25-pull-request--code-review-rules)
26. [Integration Rules](#26-integration-rules)
27. [Change Management / Contract Modification Rules](#27-change-management--contract-modification-rules)
28. [Testing Requirements](#28-testing-requirements)
29. [Deployment Requirements](#29-deployment-requirements)
30. [Definition of Done](#30-definition-of-done)
31. [Open Decisions / Items to Finalize](#31-open-decisions--items-to-finalize)
32. [Pre-Development Decisions Checklist](#32-pre-development-decisions-checklist)

---

## 1. Project Overview

We are building a **reusable, plug-and-play Agentic RAG (Retrieval-Augmented Generation) chatbot component** that any host application can embed. A host application (or its user) provides authorized data — PDF, CSV, Excel, JSON, or other agreed formats — and end users can then ask natural-language questions. The system retrieves relevant information from that authorized data and uses an LLM to generate a grounded answer, with source citations and validation to reduce hallucination.

**Core technology commitments (fixed by Team Lead):**
- Backend: **C++** (required)
- Python: **permitted** for AI/ML/data-processing support components (embeddings, chunking, orchestration helpers), where technically useful
- Interface: **REST APIs**
- Target: **~200 ms backend processing latency** per query (backend logic only — excludes full LLM generation time; see Section 20)

Everything else not explicitly fixed above (specific frameworks, databases, LLM provider, vector store, etc.) is **undecided** and marked `TBD` throughout this document. Recommendations are provided but must be ratified by the team before Phase 1 begins.

---

## 2. Purpose of the Technical Contract

Nine people (or sub-teams) will build nine phases largely **independently and in parallel** on separate Git branches. Without an agreed contract, the following WILL happen:

- Mismatched API request/response shapes between phases
- Conflicting assumptions about authentication, session, and tenant data
- Incompatible database/vector schemas
- Divergent library versions causing build breakage
- Unclear ownership when something breaks at integration time

This document exists to **prevent those problems before they happen** by fixing every cross-phase interface, format, and responsibility in writing, so that:

- Any two phases can be developed without talking to each other, as long as both follow this contract.
- Integration into `main` becomes a matter of verification against this contract, not renegotiation.
- The Team Lead has one document to check PRs against.

**Rule:** If a phase's implementation and this contract disagree, **the contract wins**, unless the contract is formally amended (Section 27) *before* the PR is merged.

---

## 3. Scope

### In Scope
- A backend service (C++) exposing REST APIs for upload, query, auth, session, and admin operations
- Ingestion pipeline for PDF, CSV, Excel, JSON (Phase 3)
- RAG retrieval engine (Phase 4) and agentic orchestration layer (Phase 5)
- LLM integration for answer generation with citations and hallucination checks (Phase 6)
- Security hardening and latency optimization (Phase 7)
- An embeddable chatbot UI widget (Phase 8)
- Final integration, end-to-end testing, and deployment (Phase 9, owned by Team Lead)

### Out of Scope (unless later added via Section 27)
- Building a full standalone SaaS product / billing system
- Multi-language i18n for the UI (unless the team decides otherwise — see Section 31)
- Support for data formats other than PDF, CSV, Excel, JSON, and any formats explicitly agreed on later
- Training or fine-tuning custom LLMs (integration with existing LLM APIs/models only, unless changed)

---

## 4. System Architecture

### 4.1 High-Level Component Diagram

```
                                 ┌─────────────────────────────┐
                                 │        Host Application      │
                                 │   (embeds Chatbot Widget)    │
                                 └───────────────┬──────────────┘
                                                 │ REST/HTTPS
                                                 ▼
                     ┌───────────────────────────────────────────────────┐
                     │                C++ BACKEND SERVICE                 │
                     │  ┌────────────┐  ┌─────────────────────────────┐  │
                     │  │ Phase 1:   │  │ Phase 2:                     │  │
                     │  │ REST API   │◄─┤ Auth / Session / RBAC        │  │
                     │  │ Layer      │  └─────────────────────────────┘  │
                     │  └─────┬──────┘                                    │
                     │        │                                          │
                     │  ┌─────▼──────┐   ┌──────────────┐   ┌─────────┐  │
                     │  │ Phase 3:   │   │ Phase 4:     │   │Phase 5: │  │
                     │  │ Ingestion  ├──►│ RAG/Retrieval├──►│ Agentic │  │
                     │  │ Pipeline   │   │ Engine       │   │Orch.    │  │
                     │  └─────┬──────┘   └──────┬───────┘   └────┬────┘  │
                     │        │                 │                │       │
                     │        ▼                 ▼                ▼       │
                     │  ┌──────────┐     ┌─────────────┐   ┌──────────┐  │
                     │  │ Storage  │     │ Vector /    │   │ Phase 6: │  │
                     │  │ (files + │     │ Structured  │   │ LLM      │  │
                     │  │ metadata)│     │ Index (TBD) │   │Integration│ │
                     │  └──────────┘     └─────────────┘   └────┬─────┘  │
                     │                                          │        │
                     │                          ┌───────────────▼──────┐ │
                     │                          │ Phase 6/7: Response  │ │
                     │                          │ Validation + Security│ │
                     │                          └───────────────┬──────┘ │
                     └──────────────────────────────────────────┼────────┘
                                                                 │
                                                                 ▼
                                      ┌───────────────────────────────┐
                                      │  Phase 8: Chatbot UI Widget    │
                                      │  (embeddable, config-driven)   │
                                      └───────────────────────────────┘
```

### 4.2 Request Lifecycle (Query Path)

```
1. User sends query via Chatbot UI (Phase 8)
2. Request hits REST API (Phase 1) → Auth/Session validated (Phase 2)
3. Orchestrator (Phase 5) interprets the query, decides tools/steps needed
4. Retrieval Engine (Phase 4) fetches top-K relevant chunks (tenant/user-scoped)
5. Orchestrator assembles context, may re-retrieve or call additional tools
6. LLM Integration (Phase 6) generates an answer with the assembled context
7. Response Validation (Phase 6) checks groundedness and citation/source consistency
8. Response Layer (Phase 1) returns JSON to the UI, including sources
9. UI (Phase 8) renders the answer + citations
```

### 4.3 Architecture Style
- **Pattern:** Layered service with a clear API boundary; internal modules communicate through the frozen interfaces in Sections 7–17. Internal C++ modules are in-process by default.
- **Language boundary:** C++ owns the backend process and REST layer. Python is permitted only for supporting AI/ML/data-processing components. When Python is used, the default integration pattern is a **separate internal service over localhost HTTP/REST** so the C++ backend remains the stable public boundary. The Python service must never be exposed directly to the host application.
- **Architecture consistency rule:** Developers may choose internal implementation details within their phase, but may not independently change cross-phase interfaces, schemas, shared dependencies, security assumptions, or the Python/C++ boundary.
- **Statelessness:** The REST API layer should be stateless per-request; session state is externalized (Section 9), not held in backend process memory, so the service can scale horizontally.

---

## 5. Component Responsibilities

| Component | Owned By (Phase) | Responsibility | Must NOT Do |
|---|---|---|---|
| REST API Layer | Phase 1 | Route requests, validate request shape, call downstream modules, format responses | Business logic, direct DB access bypassing modules |
| Auth/Session/RBAC | Phase 2 | Validate tokens, resolve identity, enforce permissions, manage sessions | Data retrieval, answer generation |
| Ingestion Pipeline | Phase 3 | Accept files, parse, clean, chunk-ready output, store with metadata | Embedding generation, retrieval, answer generation |
| RAG/Retrieval Engine | Phase 4 | Embed/index content, perform similarity + metadata search, return ranked chunks with sources | Deciding *when* to retrieve (that's Phase 5), generating final answers |
| Agentic Orchestrator | Phase 5 | Interpret query, plan steps, call retrieval/tools, manage context, handle clarification | Directly calling the LLM for final answer generation (delegates to Phase 6), UI logic |
| LLM Integration & Response Validation | Phase 6 | Build prompts, call LLM, generate answer, validate groundedness/citation consistency, return validated answer object | Retrieval, auth, storage, security testing |
| Security/Perf | Phase 7 | Define security requirements, perform adversarial/security testing, profile latency, load test, report defects and retest fixes | Owning/fixing feature implementation; no direct changes to another phase's branch |
| Chatbot UI | Phase 8 | Render widget, handle upload/query UX, call REST API, display sources/errors | Any backend logic, direct DB/vector access |
| Integration/Deployment | Phase 9 (Team Lead) | Merge, end-to-end test, deploy | — |

---

## 6. Phase-wise Responsibilities

Each phase below lists **Inputs it depends on**, **Outputs it must produce**, and **Branch name convention** (see Section 24).

| Phase | Depends On (Interfaces, not code) | Produces (Interfaces, not code) | Branch |
|---|---|---|---|
| 1. Backend & REST API Foundation | None (foundation) | REST endpoint skeletons, request/response envelope, error format | `phase-1-backend-api` |
| 2. Auth, Authorization & Session | Phase 1 request/response envelope | Auth middleware, token validation contract, session object schema | `phase-2-auth-session` |
| 3. Data Upload & Ingestion | Phase 1 envelope, Phase 2 identity context | Stored documents + metadata records, per-chunk-ready text output | `phase-3-ingestion` |
| 4. RAG & Retrieval Engine | Phase 3 chunk output schema | Retrieval API (internal), ranked results with source refs | `phase-4-rag-retrieval` |
| 5. Agentic RAG & Orchestrator | Phase 4 retrieval API, Phase 6 LLM call contract | Orchestration flow, tool-calling contract, context object | `phase-5-agentic-orchestrator` |
| 6. LLM Integration & Validation | Phase 5 context object | Answer object with citations, validation flags | `phase-6-llm-validation` |
| 7. Security & Performance | All above (test-focused; no new product features) | Security/performance test reports, benchmark results, defect reports; fixes are implemented by owning phases and retested by Phase 7 | `phase-7-security-perf` |
| 8. Chatbot UI & Plug-and-Play | Phase 1 REST contract, Phase 2 auth contract | Embeddable widget, config schema, integration docs | `phase-8-chatbot-ui` |
| 9. Final Integration & Deployment | All phases merged to `main` | Deployed system | `main` (Team Lead only) |

> Each phase developer should treat the **interface contracts in Sections 7–17** as fixed. If a developer believes an interface needs to change, they must follow Section 27 — not silently change it in their branch.

---

## 7. API Contract

### 7.1 Base Conventions
- **Base URL:** `TBD` (e.g., `https://api.<project>.example.com/v1`) — placeholder `/api/v1` used below.
- **Format:** All request/response bodies are `application/json` unless uploading files (`multipart/form-data`).
- **Versioning:** All endpoints prefixed with `/v1`. Breaking changes require `/v2`, not silent modification.
- **Encoding:** UTF-8 only.
- **Time format:** ISO 8601 UTC (e.g., `2026-09-09T14:22:00Z`) everywhere.
- **IDs:** UUID v4 strings for all resource IDs unless stated otherwise.

### 7.2 Endpoint Inventory

| Method | Path | Phase Owner | Auth Required | Purpose |
|---|---|---|---|---|
| POST | `/v1/auth/login` | 2 | No | Authenticate user, issue token |
| POST | `/v1/auth/refresh` | 2 | Yes (refresh token) | Refresh access token |
| POST | `/v1/auth/logout` | 2 | Yes | Invalidate session |
| POST | `/v1/data/upload` | 3 | Yes | Upload a data file for ingestion |
| GET | `/v1/data/{document_id}/status` | 3 | Yes | Check ingestion status |
| GET | `/v1/data` | 3 | Yes | List documents for current tenant/user |
| DELETE | `/v1/data/{document_id}` | 3 | Yes | Delete a document and its index entries |
| POST | `/v1/query` | Public API: 1; Business handler: 5; LLM: 6 | Yes | Ask a natural-language question |
| GET | `/v1/health` | 1 | No | Liveness/readiness probe |
| GET | `/v1/config` | 8 | Yes | Fetch non-secret widget configuration for host app |

> Additional endpoints may be proposed via Section 27. No developer should add undocumented endpoints without updating this table in the same PR.

### 7.3 Standard Response Envelope

**Every** successful response follows this envelope:

```json
{
  "success": true,
  "data": { },
  "meta": {
    "request_id": "uuid-string",
    "timestamp": "2026-09-09T14:22:00Z"
  }
}
```

**Every** error response follows this envelope (see Section 18 for full error contract):

```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE_STRING",
    "message": "Human-readable message",
    "details": { }
  },
  "meta": {
    "request_id": "uuid-string",
    "timestamp": "2026-09-09T14:22:00Z"
  }
}
```

**Rule:** No phase may return a raw, non-enveloped JSON body from a public endpoint. This is mandatory so Phase 8 (UI) can write one generic response handler.

---

## 8. Request and Response Schemas

### 8.1 `POST /v1/query` (Core interface — owned jointly by Phase 5 & 6, exposed by Phase 1)

**Request**

| Field | Type | Required | Notes |
|---|---|---|---|
| `query` | string | Yes | Natural-language question, max length `TBD` (recommend 2000 chars) |
| `session_id` | string (UUID) | Yes | From Phase 2 session |
| `document_ids` | array[string] | No | Restrict retrieval to specific documents; omit = search all authorized docs |
| `conversation_id` | string (UUID) | No | For multi-turn context; omitted = new conversation |
| `options` | object | No | e.g. `{ "top_k": 5, "stream": false }` |

```json
{
  "query": "What was our Q3 revenue?",
  "session_id": "a1b2c3d4-...",
  "document_ids": ["doc-uuid-1"],
  "conversation_id": "conv-uuid-1",
  "options": { "top_k": 5 }
}
```

**Response `data` object**

| Field | Type | Notes |
|---|---|---|
| `query_id` | string (UUID) | Unique ID for this Q&A turn |
| `answer` | string | Generated answer text |
| `sources` | array[object] | See below |
| `validation` | object | Groundedness/citation validation result; see Section 17 | |
| `conversation_id` | string (UUID) | Echoed / newly created |
| `latency_ms` | object | `{ "backend_ms": 180, "llm_ms": 950, "total_ms": 1130 }`; timestamp boundaries are defined in Section 20.1 |

`sources[]` item:

```json
{
  "document_id": "doc-uuid-1",
  "chunk_id": "chunk-uuid-9",
  "excerpt": "string, max ~300 chars",
  "score": 0.87,
  "page_or_location": "page 4"
}
```

### 8.2 `POST /v1/data/upload` (Owned by Phase 3)

**Request:** `multipart/form-data`

| Field | Type | Required | Notes |
|---|---|---|---|
| `file` | binary | Yes | The file itself |
| `file_type` | string enum | Yes | `pdf`, `csv`, `xlsx`, `json` (extend via Section 27) |
| `metadata` | JSON string | No | Arbitrary key-value tags, e.g. `{"department":"finance"}` |

**Response `data` object**

| Field | Type | Notes |
|---|---|---|
| `document_id` | string (UUID) | |
| `status` | string enum | `queued`, `processing`, `ready`, `failed` |
| `filename` | string | |
| `size_bytes` | integer | |
| `uploaded_at` | ISO 8601 string | |

### 8.3 Validation Rules (apply to all endpoints unless overridden)
- Reject unknown top-level fields with `400 VALIDATION_ERROR` (strict schema). Strict parsing is mandatory for the launch contract.
- All string fields trimmed of leading/trailing whitespace before validation.
- Empty `query` string → `400 VALIDATION_ERROR`.
- `document_ids`, if provided, are treated as a requested scope only; authorization is re-checked against `IdentityContext` and tenant/user permissions before retrieval. Unauthorized access returns `403 FORBIDDEN` or a non-disclosing generic `404` according to the endpoint semantics.

---

## 9. Authentication & Authorization Contract

**Owned by Phase 2.** All other phases treat this as a black box behind the interface below.

### 9.1 Authentication Mechanism
- **Type:** **JWT bearer tokens** for the launch implementation. OAuth/API-key support may be added later only through Section 27.
- **Header:** `Authorization: Bearer <token>` on every authenticated request.
- **Token contents (minimum required claims):**

```json
{
  "sub": "user-uuid",
  "tenant_id": "tenant-uuid",
  "roles": ["user", "admin"],
  "exp": 1234567890,
  "iat": 1234567890
}
```

### 9.2 Identity Context Object (internal, passed between modules)

`tenant_id` identifies the host application/tenant. Every downstream module (Ingestion, Retrieval, Orchestrator, LLM) receives an `IdentityContext`, never the raw token:

```json
{
  "user_id": "uuid",
  "tenant_id": "uuid",
  "roles": ["user"],
  "session_id": "uuid"
}
```

**Rule:** Only Phase 2 code parses/validates raw tokens. All other phases consume `IdentityContext` only. This decouples the auth mechanism (JWT vs. OAuth vs. API key — TBD) from every other phase.

### 9.3 RBAC (Role-Based Access Control)

| Role | Upload Data | Delete Data | Query | Manage Users |
|---|---|---|---|---|
| `user` | ✅ (own) | ✅ (own) | ✅ | ❌ |
| `admin` | ✅ | ✅ (tenant-wide) | ✅ | ✅ |
| `viewer` (optional, TBD) | ❌ | ❌ | ✅ | ❌ |

- Final role list and permission matrix: **TBD, must be finalized before Phase 2 coding starts.**
- Authorization failures return `403 FORBIDDEN` with code `PERMISSION_DENIED` (Section 18).

### 9.4 Session Management
- Sessions identified by `session_id` (UUID), separate from the auth token.
- Session store: `TBD` (recommend Redis or equivalent in-memory store for the ~200ms latency target; a DB-only session store may be too slow — flag for Phase 7 benchmarking).
- Session expiry: `TBD` (recommend 30–60 min sliding window).

---

## 10. User/Application/Tenant Data Isolation

This is a **mandatory, non-negotiable** requirement given the "plug-and-play, multi-host-app" nature of the project.

### 10.1 Isolation Model
- Every stored document, chunk, embedding, and conversation record **must** carry `tenant_id` and `user_id` (or `owner_id`) fields.
- Every retrieval query **must** be filtered by `tenant_id` (mandatory) and, depending on sharing rules, `user_id`.
- **Launch model:** `tenant_id` is the primary isolation boundary. Users inside a tenant may access only documents permitted by their role/ownership/sharing metadata. Admins may access tenant-wide data; ordinary users are restricted to their own/shared documents. `tenant_id` filtering is mandatory even when `user_id` filtering is also applied.

### 10.2 Enforcement Points
| Layer | Enforcement |
|---|---|
| API (Phase 1/2) | Reject requests without valid `tenant_id` context |
| Ingestion (Phase 3) | Stamp every stored record with `tenant_id`/`user_id` at write time |
| Retrieval (Phase 4) | Every query includes a mandatory metadata filter on `tenant_id` — **never** optional |
| Orchestrator (Phase 5) | Passes `IdentityContext` unchanged through the pipeline; never widens scope |

**Rule:** Retrieval code that does not apply a `tenant_id` filter is a **security defect**, not a style issue, and must block PR merge (Section 25).

---

## 11. Data Ingestion Contract

**Owned by Phase 3.**

### 11.1 Supported Formats (Phase 1 launch scope)
| Format | Library/Approach | Notes |
|---|---|---|
| PDF | `TBD` (e.g., PDF text extraction lib) | Must support text-based PDFs; scanned/OCR PDFs — `TBD` scope decision |
| CSV | `TBD` | Delimiter auto-detect recommended |
| Excel (`.xlsx`) | `TBD` | Multi-sheet handling — `TBD` |
| JSON | Native | Arbitrary nested JSON must be flattened/summarized for chunking |

### 11.2 Ingestion Pipeline Steps (contract, not implementation)
1. **Upload received** → file stored in raw form (location: `TBD`, e.g., local disk / object storage)
2. **Validation** → file type, size limit (`TBD`, recommend max 25–50 MB), corruption check
3. **Parsing** → format-specific extraction into plain text + structural metadata (e.g., page number, row/column, JSON path)
4. **Cleaning** → whitespace normalization, encoding fixes, removal of non-content boilerplate
5. **Chunking** → split into retrieval-sized chunks (chunk size/overlap: `TBD`, recommend 500–1000 tokens, 10–20% overlap)
6. **Metadata extraction** → attach `document_id`, `tenant_id`, `user_id`, `source_location`, `chunk_index`
7. **Handoff to Phase 4** via the Chunk Output Schema below. Phase 3 does **not** generate embeddings; Phase 4 owns embedding generation and indexing.

### 11.3 Chunk Output Schema (Interface: Phase 3 → Phase 4)

```json
{
  "document_id": "uuid",
  "tenant_id": "uuid",
  "chunk_id": "uuid",
  "chunk_index": 0,
  "text": "cleaned chunk text",
  "source_location": "page 3 | row 12 | $.orders[4]",
  "metadata": { "filename": "report.pdf", "uploaded_by": "uuid" }
}
```

- **Input:** raw uploaded file + `IdentityContext`
- **Output:** array of the above chunk objects
- **Required fields:** `document_id`, `tenant_id`, `chunk_id`, `text`
- **Optional fields:** `source_location`, arbitrary `metadata`
- **Validation:** `text` must not be empty; reject chunks over max token size (`TBD`)
- **Error conditions:** `UNSUPPORTED_FILE_TYPE`, `FILE_TOO_LARGE`, `PARSE_FAILURE`, `EMPTY_DOCUMENT`
- **Auth:** requires valid `IdentityContext`; ingestion always tenant/user-scoped

### 11.4 Ingestion Status Values
`queued` → `processing` → `ready` | `failed` (with `error` details attached)

---

## 12. Data Schema Contract

> **Explicitly TBD:** Final database engine(s) are not decided (Section 31). This section defines the **logical schema** every phase must respect regardless of physical storage choice.

### 12.1 Logical Entities

**Document**
| Field | Type | Required |
|---|---|---|
| `document_id` | UUID | Yes |
| `tenant_id` | UUID | Yes |
| `owner_user_id` | UUID | Yes |
| `filename` | string | Yes |
| `file_type` | enum | Yes |
| `status` | enum | Yes |
| `uploaded_at` | timestamp | Yes |
| `metadata` | JSON/object | No |

**Chunk** (see 11.3 — same fields, plus embedding reference)
| Field | Type | Required |
|---|---|---|
| `chunk_id` | UUID | Yes |
| `document_id` | UUID | Yes |
| `tenant_id` | UUID | Yes |
| `text` | string | Yes |
| `embedding_ref` | string/vector-id | No (created/attached by Phase 4) |

**Conversation / Query Log**
| Field | Type | Required |
|---|---|---|
| `conversation_id` | UUID | Yes |
| `tenant_id` | UUID | Yes |
| `user_id` | UUID | Yes |
| `turns` | array of `{query, answer, sources, timestamp}` | Yes |

### 12.2 Storage Technology Decisions — ALL TBD
| Concern | Options (recommendations, not decisions) | Status |
|---|---|---|
| Relational metadata store | PostgreSQL / SQLite (dev) / MySQL | **TBD** |
| Vector store | e.g., a dedicated vector DB, or a library-based local index | **TBD** |
| File/object storage | Local disk (dev) / S3-compatible store | **TBD** |
| Session store | Redis or equivalent | **TBD** |

**Rule:** Phase 3 and Phase 4 code must access storage through an internal abstraction/interface (repository pattern), **not** hardcoded SQL/vector-DB calls scattered through business logic — this allows the final storage decision to land later without rewriting Phase 3–6 logic.

---

## 13. RAG & Retrieval Contract

**Owned by Phase 4.**

### 13.1 Retrieval Interface (Internal API: Phase 5 → Phase 4)

**Input**
```json
{
  "query_text": "string",
  "tenant_id": "uuid",
  "user_id": "uuid",
  "document_ids": ["uuid"],
  "top_k": 5,
  "filters": { "metadata_key": "value" }
}
```

**Output**
```json
{
  "results": [
    {
      "chunk_id": "uuid",
      "document_id": "uuid",
      "text": "chunk content",
      "score": 0.91,
      "source_location": "page 4"
    }
  ]
}
```

- **Required fields (input):** `query_text`, `tenant_id`
- **Optional fields (input):** `document_ids`, `top_k` (default `TBD`, recommend 5), `filters`
- **Validation:** `top_k` capped at a max (`TBD`, recommend 20) to protect latency budget
- **Error conditions:** `EMPTY_QUERY`, `NO_DOCUMENTS_INDEXED`, `RETRIEVAL_TIMEOUT`
- **Auth:** Caller must supply valid `tenant_id`; retrieval layer enforces the isolation filter regardless of what caller passes (defense in depth)
- **Phase ownership:** Phase 4 owns implementation; Phase 5 is the only internal caller (Phase 6 never calls retrieval directly)

### 13.2 Embedding & Indexing
- Embedding model: `TBD` (Section 31) — must be documented once chosen, including dimension size, since it affects the vector store choice.
- Similarity metric: `TBD` (recommend cosine similarity as default).
- Re-indexing strategy on document update/delete: must be defined before Phase 4 completion (delete stale vectors, avoid duplicate chunks).

### 13.3 Source Tracking
Every retrieved chunk **must** carry enough metadata to reconstruct a human-readable citation (`source_location` + `filename`), because Phase 6 depends on this for citation generation and Phase 8 depends on it for UI display.

---

## 14. Agentic RAG Contract

**Owned by Phase 5.**

### 14.1 Responsibilities
- Interpret the user's query (intent, whether retrieval is even needed, whether clarification is needed).
- Decide which tool(s) to call (retrieval, structured-data queries for CSV/Excel/JSON, calculators, and future tools).
- Route numeric/aggregation questions over structured files to `structured_query` rather than relying only on vector similarity retrieval.
- Support **multi-step workflows**: e.g., retrieve → find gap → re-retrieve with refined query → send to LLM.
- Maintain a **context object** across steps within a single query turn (and across turns if `conversation_id` is provided).

### 14.2 Orchestrator Context Object (Internal — passed to Phase 6)

```json
{
  "query_id": "uuid",
  "identity": { "user_id": "uuid", "tenant_id": "uuid" },
  "original_query": "string",
  "refined_query": "string",
  "retrieved_chunks": [ /* Chunk objects from 13.1 output */ ],
  "steps_taken": [
    { "step": "retrieve", "tool": "vector_search", "result_summary": "5 chunks found" }
  ],
  "conversation_history": [ /* prior turns, optional */ ]
}
```

- **Required fields:** `query_id`, `identity`, `original_query`, `retrieved_chunks`
- **Optional fields:** `refined_query`, `steps_taken`, `conversation_history`
- **Error/failure handling:** If retrieval returns zero relevant chunks (score below threshold `TBD`), orchestrator must set a flag (`"needs_clarification": true`) rather than pass empty context silently to the LLM.
- **Phase ownership:** Phase 5 produces this object; Phase 6 consumes it and must not re-query retrieval itself.

### 14.3 Clarification & Failure Handling
- If confidence is low or the query is ambiguous, the orchestrator returns a clarification request instead of calling the LLM for a final answer:

```json
{ "needs_clarification": true, "clarification_prompt": "Could you specify which quarter you mean?" }
```

- Max retrieval attempts per query turn: `TBD` (recommend 2, to protect the latency budget).

---

## 15. Tool-Calling Contract

**Owned by Phase 5** (definitions), **consumed by** any phase implementing a callable tool.

### 15.1 Tool Interface (all tools must conform)

```json
{
  "tool_name": "string",
  "input": { },
  "output": { },
  "error": null
}
```

| Field | Type | Required |
|---|---|---|
| `tool_name` | string | Yes |
| `input` | object (tool-specific) | Yes |
| `output` | object (tool-specific) | Yes on success |
| `error` | object or null | Yes (null on success) |

### 15.2 Registered Tools (launch scope)

| Tool Name | Owner Phase | Input Summary | Output Summary |
|---|---|---|---|
| `vector_search` | 4 | query, filters, top_k | ranked chunks |
| `structured_query` (optional, for CSV/Excel numeric lookups) | 4 (or new, TBD) | query, document_ids | tabular result rows |
| *(future tools)* | TBD | — | — |

**Rule:** Any new tool added after launch must be registered in this table via Section 27 before being called by the orchestrator — the orchestrator must not call undocumented tools.

---

## 16. LLM Integration Contract

**Owned by Phase 6.**

### 16.1 LLM Provider/Model
- **TBD** — must be selected by the team (Section 31). Do not hardcode a specific model name/version in orchestration or UI code; isolate it behind an internal `LLMClient` interface so it can be swapped.

### 16.2 LLM Call Interface (Internal: Phase 6 wraps this; Phase 5 never calls the LLM directly)

**Input**
```json
{
  "system_prompt": "string",
  "context_chunks": [ /* from Orchestrator Context Object */ ],
  "conversation_history": [ ],
  "user_query": "string",
  "generation_params": { "temperature": 0.2, "max_tokens": 500 }
}
```

**Output**
```json
{
  "answer_text": "string",
  "citations": [ { "chunk_id": "uuid", "used": true } ],
  "raw_model_metadata": { "model": "TBD", "tokens_used": 342 }
}
```

- **Required fields (input):** `context_chunks`, `user_query`
- **Optional fields (input):** `conversation_history`, `generation_params`
- **Validation:** `context_chunks` should not exceed the model's context window (compute a token budget; exact numbers `TBD` once model is chosen)
- **Error conditions:** `LLM_TIMEOUT`, `LLM_RATE_LIMIT`, `LLM_CONTENT_FILTERED`, `LLM_PROVIDER_ERROR`
- **Auth:** LLM API keys are backend-only secrets — **never** sent to or exposed in the Chatbot UI (Phase 8) or client-side code (Section 19)

### 16.3 Prompt Template Contract
- Prompt templates live in a dedicated, version-controlled location (`TBD` path — see Section 23) so Phase 7 can test prompt-injection resistance without touching orchestration logic.
- Every prompt **must** instruct the model to cite only from provided context and to state when it cannot answer from the given data — this is required for Section 17 (Response Validation).

---

## 17. Response Validation Contract

**Owned by Phase 6.**

### 17.1 Validation Object (attached to every answer)

```json
{
  "groundedness": "high",
  "unsupported_claims": [],
  "citation_coverage": 0.92,
  "passed": true
}
```

| Field | Type | Meaning |
|---|---|---|
| `groundedness` | enum: `low`/`medium`/`high` | Estimated degree to which answer claims are supported by authorized retrieved/tool results |
| `unsupported_claims` | array[string] | Sentences/claims in the answer not traceable to retrieved chunks |
| `citation_coverage` | float 0–1 | Fraction of answer content backed by cited sources |
| `passed` | boolean | Whether the answer meets the minimum bar to be shown as-is |

### 17.2 Validation Rules
- If `passed` is `false`, Phase 6 must either (a) attempt one regeneration with a stricter prompt, or (b) fall back to a clarification/"I don't have enough information" response — never silently show an unvalidated answer as if it were confident.
- Launch validation must include deterministic citation/source consistency checks. An optional second-LLM judge may be added later, but must not be the sole authority for authorization or source identity.
- Phase 4 provides authoritative `chunk_id`, `document_id`, `filename`, and `source_location` metadata. Phase 6 maps answer claims/citations to these retrieved records; the LLM must not invent source identifiers.

---

## 18. Error Handling Contract

### 18.1 Standard Error Codes (extend via Section 27, do not invent ad hoc codes)

| Code | HTTP Status | Meaning |
|---|---|---|
| `VALIDATION_ERROR` | 400 | Malformed/missing request fields |
| `UNAUTHENTICATED` | 401 | Missing/invalid/expired token |
| `PERMISSION_DENIED` | 403 | Valid identity, insufficient role/ownership |
| `NOT_FOUND` | 404 | Resource does not exist (or not visible to this tenant) |
| `UNSUPPORTED_FILE_TYPE` | 415 | Upload format not supported |
| `FILE_TOO_LARGE` | 413 | Exceeds size limit |
| `RATE_LIMITED` | 429 | Too many requests |
| `RETRIEVAL_TIMEOUT` | 504 | Retrieval exceeded time budget |
| `LLM_TIMEOUT` / `LLM_PROVIDER_ERROR` | 502/504 | LLM call failed |
| `INTERNAL_ERROR` | 500 | Unhandled server error |

### 18.2 Error Response Shape
See Section 7.3. `details` is optional and should never leak stack traces or internal paths in production (only in a `debug` mode gated by environment config — Section 22).

### 18.3 Rule for All Phases
Every phase must throw/return **only** documented error codes at the API boundary. Internal exceptions must be caught and translated at the Phase 1 API layer boundary before leaving the backend.

---

## 19. Security Requirements

**Primarily driven by Phase 7, but binding on all phases from day one.**

### 19.1 Mandatory Requirements (all phases)
- No secrets (API keys, DB credentials, LLM keys) in source code, commit history, or client-side (Phase 8) code. Use environment variables (Section 22).
- All endpoints except `/v1/auth/login` and `/v1/health` require authentication.
- All tenant-scoped queries enforce `tenant_id` filtering at the data-access layer (Section 10), not just at the API layer.
- Input sanitization on all user-provided text before it reaches file parsers, DB queries, or the LLM prompt.

### 19.2 Prompt Injection Defense (Phase 6/7)
- Treat all retrieved document content as **untrusted input** to the LLM — never as trusted system instructions.
- System prompt must explicitly instruct the model to ignore instructions found inside retrieved content.
- Phase 7 must include a prompt-injection test suite (e.g., documents containing "ignore previous instructions" style content) before Phase 6 is marked done.

### 19.3 Data Leakage Prevention
- Cross-tenant retrieval must be tested explicitly (Phase 7): attempt to retrieve another tenant's data and confirm it's blocked.
- Error messages must not reveal whether a resource exists for another tenant (return `403`/generic `404`, not information-disclosing messages).

### 19.4 Transport & Storage Security
- HTTPS/TLS required in all non-local environments — `TBD` for certificate management approach.
- Passwords (if applicable) hashed with a strong algorithm (e.g., bcrypt/argon2) — never stored plaintext. Exact library `TBD`.
- File uploads must be validated using extension + declared MIME type + file signature/magic bytes where applicable before parsing.
- Filenames must be sanitized; path traversal and unsafe paths must be rejected.
- Parsers must enforce resource/time limits to reduce denial-of-service risk from malformed files.
- Phase 7 must include malformed/malicious file tests.

---

## 20. Performance & 200 ms Backend Latency Contract

### 20.1 Definition of the 200 ms Target

The **200 ms target applies to backend processing latency excluding LLM inference/generation**. The timestamps are defined as:

```text
t0 = request received by Phase 1
t1 = request/context handed to the LLM client
t2 = LLM response received
t3 = final response sent by Phase 1

backend_pre_llm_ms = t1 - t0
llm_ms            = t2 - t1
backend_post_llm_ms = t3 - t2
total_ms          = t3 - t0
backend_ms        = backend_pre_llm_ms + backend_post_llm_ms
```

The launch performance target is **P95 `backend_ms` ≤ 200 ms** under the benchmark conditions defined by Phase 7. LLM inference/network time is reported separately in `llm_ms` and is not included in the 200 ms backend target.

```text
total_ms = backend_ms + llm_ms
```

### 20.2 What Counts Toward the 200 ms Budget
- Auth/session validation (Phase 2)
- Query validation and serialization/deserialization (Phase 1)
- Retrieval, including query embedding and vector/structured search (Phase 4)
- Orchestration overhead and tool coordination (Phase 5), excluding LLM inference/network time
- Response validation/formatting performed after the LLM returns (Phase 6)
- Final response formatting by Phase 1

### 20.3 What Is Measured Separately
- LLM network/inference/generation time — `latency_ms.llm_ms`
- File ingestion/upload processing — outside the query-path budget

### 20.4 Measurement Requirements (Phase 7)
- Backend must record `backend_ms`, `llm_ms`, and `total_ms` for every query in structured logs; these fields are also returned in the API response where enabled by the public contract.
- Phase 7 must produce **P50 / P95 / P99** results.
- The report must document hardware, dataset/index size, concurrency, query mix, cache state, and whether the LLM was mocked or real.
- The 200 ms target is a benchmark target, not a guarantee for arbitrary production environments.

### 20.5 Optimization Guidance (non-binding, Phase 7 to refine)
- Use connection pooling for DB/vector store calls.
- Cache embeddings for repeated/identical queries where safe to do so.
- Avoid synchronous blocking calls in the C++ request path; consider async I/O for DB/vector calls (implementation detail, Phase 1/4 to align).

---

## 21. Dependencies & Version Management

### 21.1 Rules (mandatory for all phases)
- Every phase must declare its dependencies explicitly in the relevant manifest (C++: `TBD` — e.g., `vcpkg.json`/`conanfile`; Python: `requirements.txt` or `pyproject.toml`).
- **No developer may bump a shared dependency's version without raising it in the team channel and updating this section** — shared dependency conflicts are the #1 cause of "works on my branch, fails at integration."
- Pin exact versions (not floating `^`/`latest`) for anything used across phase boundaries.

### 21.2 Shared/Cross-Phase Dependencies (fill in as decided)

| Dependency | Purpose | Version | Used By Phases | Status |
|---|---|---|---|---|
| C++ compiler/standard | Backend language | `TBD` (recommend C++17 or newer) | All backend | TBD |
| REST framework (C++) | API layer | `TBD` | 1 | TBD |
| JSON library (C++) | Serialization | `TBD` (e.g., nlohmann/json) | 1, all | TBD |
| Python interpreter | AI/ML support code | `TBD` (recommend 3.11+) | 3, 4, 6 (if Python used) | TBD |
| Embedding library/model | Vector generation | `TBD` | 4 | TBD |
| LLM SDK | LLM calls | `TBD` | 6 | TBD |
| Vector DB client | Retrieval | `TBD` | 4 | TBD |
| Testing framework (C++) | Unit tests | `TBD` (e.g., GoogleTest) | All | TBD |
| Testing framework (Python) | Unit tests | `TBD` (e.g., pytest) | 3, 4, 6 | TBD |

---

## 22. Configuration & Environment Variables

### 22.1 Rules
- All configuration (URLs, keys, timeouts, feature flags) via environment variables or a config file **excluded from version control** (`.env` pattern). Never hardcode.
- Provide a `.env.example` (committed) listing all required variable **names** with placeholder values, updated by whichever phase introduces a new variable.

### 22.2 Known Required Variables (grows as phases add their own)

| Variable | Owned By Phase | Purpose |
|---|---|---|
| `BACKEND_PORT` | 1 | Port for REST API |
| `JWT_SECRET` / `AUTH_PROVIDER_CONFIG` | 2 | Token signing/validation |
| `SESSION_STORE_URL` | 2 | Session store connection |
| `FILE_STORAGE_PATH` / `OBJECT_STORE_URL` | 3 | Uploaded file storage |
| `DB_CONNECTION_STRING` | 3, 4 | Metadata DB |
| `VECTOR_DB_URL` | 4 | Vector store connection |
| `LLM_API_KEY` | 6 | LLM provider credential |
| `LLM_MODEL_NAME` | 6 | Model identifier |
| `LOG_LEVEL` | All | Logging verbosity |
| `ENVIRONMENT` | All | `development` / `staging` / `production` |

---

## 23. Project/Folder Structure

> Structure below is a **recommended baseline** to minimize merge conflicts between phases working in parallel. Confirm/adjust in Section 31 before Phase 1 starts, then treat as fixed.

```
/plug-and-play-agentic-rag/
├── TECHNICAL_CONTRACT.md
├── README.md
├── .env.example
├── backend/                     # C++ backend (Phase 1 root)
│   ├── src/
│   │   ├── api/                 # Phase 1: routes, controllers
│   │   ├── auth/                # Phase 2
│   │   ├── ingestion/           # Phase 3
│   │   ├── retrieval/           # Phase 4
│   │   ├── orchestrator/        # Phase 5
│   │   ├── llm/                 # Phase 6
│   │   ├── common/              # Team Lead controlled: shared contracts, envelope, errors, IdentityContext
│   │   └── main.cpp
│   ├── tests/                   # Mirrors src/ structure
│   └── CMakeLists.txt (or build config, TBD)
├── python_services/              # Only if Python is used as a separate process (Section 4.3 decision)
│   ├── embeddings/
│   ├── ingestion_helpers/
│   └── requirements.txt
├── prompts/                      # Phase 6 prompt templates (version controlled)
├── widget/                       # Phase 8: Chatbot UI
│   ├── src/
│   └── package.json (if JS-based, TBD)
├── docs/
│   ├── integration_guide.md      # Phase 8 deliverable
│   └── api_reference.md          # Generated/maintained alongside Section 7-8
├── scripts/                       # Build/deploy/test helper scripts
└── .github/workflows/             # CI (Section 28/29)
```

**Rule:** Each phase works almost exclusively inside its own subfolder (`src/<phase-module>/`). `common/` is **Team Lead controlled**; phase developers consume shared contracts but do not modify them without an approved Section 27 change. This minimizes both Git merge conflicts and architectural/interface drift.

---

## 24. Git & Branching Strategy

### 24.1 Branch Model
- `main` — protected, always deployable, **only the Team Lead merges into it**.
- One long-lived branch per phase, named per Section 6 table (e.g., `phase-3-ingestion`).
- Developers create short-lived feature/fix branches off their phase branch if working in sub-teams (e.g., `phase-3-ingestion/pdf-parser`), then PR into the phase branch, then the phase branch PRs into `main` once the phase is complete and reviewed.

```
main
 ├── phase-1-backend-api
 ├── phase-2-auth-session
 ├── phase-3-ingestion
 │     └── phase-3-ingestion/pdf-parser   (optional sub-branch)
 ├── phase-4-rag-retrieval
 ├── phase-5-agentic-orchestrator
 ├── phase-6-llm-validation
 ├── phase-7-security-perf
 └── phase-8-chatbot-ui
```

### 24.2 Commit Convention
Recommend **Conventional Commits**: `feat(phase3): add CSV parser`, `fix(phase4): correct top_k default`. Final choice `TBD`, but must be consistent once chosen.

### 24.3 Rules
- Never commit directly to `main`.
- Synchronize your phase branch with `main` regularly (at least weekly) to catch integration conflicts early, not just at the end. Prefer merging `main` into the phase branch unless the Team Lead explicitly requires a rebase; do not rewrite shared branch history casually.
- No force-pushing to shared phase branches without notifying the sub-team.

---

## 25. Pull Request & Code Review Rules

### 25.1 PR Requirements (all phases)
- PR description must state: what was implemented, which contract sections it touches, and confirmation it does not deviate from this contract (or references the Section 27 amendment if it does).
- PR must include/update tests for new logic (Section 28).
- PR must pass CI (build + tests + lint) before review.
- No PR may introduce an undocumented API field, error code, or endpoint (Sections 7, 8, 18).

### 25.2 Review Process
1. Developer opens PR from feature branch → phase branch (or phase branch → `main`).
2. Team Lead (or designated reviewer) checks the PR against this contract.
3. If violations found: Team Lead requests changes, citing the specific section number.
4. Security/performance defects found by Phase 7 are fixed by the owning phase; Phase 7 retests and reports the result. Phase 7 does not directly modify another phase's branch.
4. Once approved: Team Lead merges (only Team Lead merges into `main`, per Section 24).

### 25.3 Merge Blockers (must-fix, non-negotiable)
- Missing `tenant_id` filtering on any data-access code (Section 10).
- Secrets committed to the repo (Section 19/22).
- Response not following the standard envelope (Section 7.3).
- Breaking an already-agreed interface without a Section 27 amendment.

---

## 26. Integration Rules

- Interfaces defined in Sections 7, 8, 11.3, 13.1, 14.2, 15.1, 16.2, 17.1 are **frozen contracts** between phases. A phase may change its *internal* implementation freely, as long as these boundary shapes are respected.
- Before requesting integration into `main`, each phase must provide:
  - A short **README** in its module folder describing how to run/test it in isolation.
  - Example request/response payloads matching this contract (for manual verification).
  - Unit tests covering its module (Section 28).
- Contract tests are required at phase boundaries before final integration: Phase 3 → 4 chunk schema; Phase 4 → 5 retrieval response; Phase 5 → 6 context object; Phase 6 → 1 final answer/response schema.
- The Team Lead performs integration testing (Phase 9) by wiring real phase implementations together and testing the full request lifecycle (Section 4.2) end-to-end.
- Integration checkpoints should occur after Phase 2, after Phase 4, after Phase 6, and after Phase 8; do not wait until Phase 9 to discover interface incompatibilities.
- If two phases' implementations disagree about a shared interface, **the contract is the tiebreaker** — the phase that deviated must fix its implementation, unless the team agrees (Section 27) to amend the contract instead.

---

## 27. Change Management / Contract Modification Rules

This contract is **not immutable**, but changes must be controlled:

1. **Propose:** Anyone who needs a change opens an issue/discussion describing the current contract text, the proposed change, and which phases are affected.
2. **Review:** Team Lead + affected phase owners review impact (does it break an already-implemented interface?).
3. **Approve:** Team Lead approves and merges the change into `TECHNICAL_CONTRACT.md` on `main` (via its own PR, reviewed like code).
4. **Notify:** Team Lead notifies all phase owners of the change and the required follow-up (e.g., "Phase 4 and Phase 6 must update to the new chunk schema by <date>").
5. **Version bump:** Increment the document version number at the top of this file and add a changelog entry below.

### 27.1 Changelog
| Version | Date | Change | Approved By |
|---|---|---|---|
| 1.0 | 2026-09-09 | Initial contract created | Team Lead |
| 1.1 | 2026-09-10 | Clarified architecture boundaries, tenant isolation, JWT launch contract, Phase 3→4 handoff, structured-data routing, groundedness validation, file security, P95 latency definition, Git/integration rules, contract tests, and launch decisions | Team Lead |

---

## 28. Testing Requirements

### 28.1 Per-Phase Minimum Requirements
| Phase | Required Tests |
|---|---|
| 1 | Endpoint routing, request validation, error envelope format |
| 2 | Token validation (valid/expired/malformed), RBAC permission matrix, session expiry |
| 3 | Each file type parses correctly; corrupt/oversized files rejected; chunk schema conformance |
| 4 | Retrieval returns correctly ranked/filtered results; tenant isolation enforced; empty-result handling |
| 5 | Orchestration flow steps; clarification triggering; multi-step re-retrieval |
| 6 | Prompt formatting; citation attachment; validation object correctness on known test cases |
| 7 | Security test suite (Section 19) + load test producing P50/P95/P99 (Section 20) |
| 8 | UI renders answers/sources/errors correctly against mock API responses matching this contract |

### 28.2 Cross-Phase / Integration Tests (Phase 9, Team Lead)
- Full request lifecycle test (upload → ingest → query → answer → citation displayed).
- Cross-tenant data leakage test.
- Latency budget test against Section 20 targets.

### 28.3 Tooling
- C++ test framework: `TBD` (recommend GoogleTest).
- Python test framework (if applicable): `TBD` (recommend pytest).
- CI: `TBD` (recommend GitHub Actions, given the repo is on GitHub).

---

## 29. Deployment Requirements

- **Target environment(s):** `TBD` (local/on-prem, cloud provider, or hybrid — Section 31).
- **Containerization:** Recommend Docker for reproducible builds across phase branches, even before final cloud target is chosen — `TBD` to confirm.
- **CI/CD:** Recommend GitHub Actions pipeline: build → test → (on `main` merge) deploy to a staging environment. Final pipeline design `TBD`.
- **Secrets in deployment:** Injected via the deployment platform's secret manager — never baked into images or committed configs.
- **Rollback plan:** `TBD` — must be defined before first production deployment (Phase 9).

---

## 30. Definition of Done

A phase is considered **Done** only when **all** of the following are true:

- [ ] All interfaces owned by the phase match this contract exactly (or contract was amended per Section 27).
- [ ] Unit tests written and passing (Section 28) with CI green.
- [ ] Module README exists explaining how to run/test it standalone.
- [ ] No hardcoded secrets; all config via environment variables (Section 22).
- [ ] Tenant/user isolation enforced wherever data is read or written (Section 10), if applicable to the phase.
- [ ] Error handling uses only documented error codes (Section 18).
- [ ] Boundary contract tests pass for every interface owned by the phase (Section 26).
- [ ] Team Lead has reviewed, tested, and approved the PR into `main`.
- [ ] For Phase 7 specifically: security test report and P50/P95/P99 latency report delivered, including benchmark conditions.
- [ ] For Phase 8 specifically: integration documentation delivered (Section 5) so a host app developer could integrate the widget using only that doc.

---

## 31. Open Decisions / Items to Finalize

The following decisions are intentionally left open because they depend on the team's available environment or final benchmarking. Items marked **FIXED** are part of the launch contract and must not be changed informally.

| # | Decision | Status | Blocks Phase(s) | Recommendation / Rule |
|---|---|---|---|---|
| 1 | C++ REST framework | TBD | 1 | Choose one lightweight, production-capable framework and pin the version before Phase 1 implementation. |
| 2 | Python/C++ boundary | **FIXED** | 3, 4, 6 | Python support components use an internal localhost HTTP/REST service; C++ remains the public backend. |
| 3 | Authentication | **FIXED** | 2 | JWT bearer tokens for launch. |
| 4 | Isolation model | **FIXED** | 3, 4, 5 | Tenant-level isolation with role/ownership-based user restrictions. |
| 5 | Relational DB | TBD | 3, 4 | Prefer PostgreSQL; SQLite may be used only for isolated local development if schemas remain compatible. |
| 6 | Vector store | TBD | 4 | Select based on filtering, persistence, C++/Python integration, and latency. |
| 7 | Embedding model | TBD | 4 | Must be pinned with its dimension and preprocessing configuration. |
| 8 | LLM provider/model | TBD | 6 | Select based on cost, latency, context window, availability, and deployment constraints. |
| 9 | Chunk size/overlap | TBD | 3, 4 | Start with 500–1000 tokens and 10–20% overlap; benchmark before finalizing. |
| 10 | Groundedness validation method | **FIXED baseline** | 6, 7 | Deterministic source/citation consistency checks are mandatory; optional LLM judge is supplementary only. |
| 11 | Session store | TBD | 2 | Prefer Redis or equivalent if benchmark results justify it. |
| 12 | Load testing tool | TBD | 7 | Select a standard HTTP load-testing tool available in CI. |
| 13 | Deployment target | TBD | 9 | Choose based on team resources/credits and required demo accessibility. |
| 14 | JSON request parsing | **FIXED** | 1 | Strict validation; reject unknown top-level fields. |
| 15 | Maximum file size | TBD | 3 | Start at 25–50 MB and enforce the value consistently. |
| 16 | Role list | **FIXED baseline** | 2 | Launch with `user` and `admin`; add roles only through Section 27. |
| 17 | Commit convention | **FIXED** | All | Conventional Commits. |
| 18 | CI/CD platform | **FIXED baseline** | 7, 9 | GitHub Actions unless repository constraints require otherwise. |
| 19 | C++ build system | **FIXED** | 1 | CMake. |
| 20 | Widget tech stack | TBD | 8 | Choose based on frontend requirements and embedding constraints. |
| 21 | Query mode | **FIXED baseline** | 1, 5, 6 | Synchronous query path for launch; async query retrieval endpoint is not part of the launch API. |
| 22 | Contract test tooling | TBD | All | Use the team's existing C++/Python test stack; boundary tests are mandatory. |

> **Rule:** Any remaining TBD that affects a phase's interface must be resolved before that phase implements the affected interface. A phase must not make a private decision that changes another phase's contract.

---

## 32. Pre-Development Decisions Checklist

> **No phase should start implementation of an affected interface until the relevant items below are checked off.** Team Lead owns this checklist.

### Architecture & Language
- [ ] Confirm C++ standard/version and CMake toolchain
- [x] Confirm Python/C++ integration pattern: internal localhost HTTP/REST service
- [ ] Confirm REST framework for C++ — Item 1
- [ ] Confirm overall folder structure (Section 23)
- [x] Confirm cross-phase rule: internal implementation freedom, frozen external interfaces

### Auth & Identity
- [x] Authentication mechanism: JWT bearer tokens
- [x] Launch role list: `user`, `admin`
- [ ] Choose session store technology — Item 11
- [ ] Decide session expiry policy
- [x] Define `tenant_id` as the host-application tenant boundary

### Data & Storage
- [x] Tenant-level isolation with user/ownership restrictions
- [ ] Choose relational DB engine — Item 5
- [ ] Choose vector store — Item 6
- [ ] Choose embedding model and dimensionality — Item 7
- [ ] Decide file/object storage approach
- [ ] Set maximum file upload size — Item 15
- [x] Launch formats: PDF, CSV, Excel (`.xlsx`), JSON
- [x] Phase 3 does not generate embeddings; Phase 4 owns embedding/indexing

### RAG & Agentic Behavior
- [ ] Set default/max `top_k`
- [ ] Set final chunk size and overlap
- [ ] Set similarity metric
- [ ] Define confidence threshold for clarification
- [x] Maximum launch re-retrieval attempts: 2 unless amended
- [x] Structured-data questions may route to `structured_query`

### LLM & Validation
- [ ] Choose LLM provider/model — Item 8
- [ ] Define token/context-window budget
- [x] Deterministic source/citation consistency validation is mandatory
- [ ] Define optional secondary validation method
- [ ] Draft and store initial system prompt template

### API & Error Contract
- [ ] Confirm base URL / hosting path
- [x] Strict request validation
- [ ] Confirm final error code list matches Section 18
- [x] Launch query API is synchronous; no async `GET /v1/query/{query_id}` endpoint

### Security
- [ ] Confirm secrets management for local dev and CI
- [ ] Confirm TLS/HTTPS approach per environment
- [ ] Confirm password hashing library if passwords are used
- [x] File signature/MIME validation, filename sanitization, and parser resource limits required

### Performance
- [x] 200 ms target defined as P95 `backend_ms` ≤ 200 ms under documented benchmark conditions
- [x] `backend_ms`, `llm_ms`, and `total_ms` timestamp boundaries are defined in Section 20.1
- [ ] Choose load testing tool — Item 12

### Process & Git
- [x] Conventional Commits
- [x] Main branch protected; Team Lead merges into `main`
- [x] `common/` is Team Lead controlled
- [x] Phase 7 reports/fixes through owning phases; it does not modify their branches directly
- [ ] Confirm PR review turnaround expectations
- [x] GitHub Actions baseline for CI/CD

### Integration
- [x] Contract tests required at Phase 3→4, 4→5, 5→6, and 6→1 boundaries
- [x] Integration checkpoints occur before Phase 9
- [x] Every phase must provide standalone README, examples, tests, and dependency manifest

### UI/Widget
- [ ] Choose widget tech stack — Item 20
- [ ] Agree on widget configuration schema
- [x] `/v1/config` may expose only non-secret UI configuration

### Deployment
- [ ] Choose deployment target/platform — Item 13
- [ ] Define rollback plan

---

### 32.1 Team Architecture Rule — Mandatory

> **Developers are free to choose internal implementation details within their assigned phase, but they cannot independently change cross-phase interfaces, schemas, shared dependencies, security assumptions, or architecture.**

If an implementation decision affects another phase, the decision must be raised through Section 27 before it is merged.

---

*End of TECHNICAL_CONTRACT.md — Version 1.1*

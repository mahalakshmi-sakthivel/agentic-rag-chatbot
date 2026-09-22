# Example Payloads — Phase 1 Endpoints

Committed for other phase owners to verify against (Definition of Done,
Section 15). All of these were run against the live server during
verification — see backend/README.md.

## GET /v1/health

```bash
curl http://localhost:8080/v1/health
```

```json
{
  "success": true,
  "data": { "status": "ok", "uptime_seconds": 2, "version": "0.1.0" },
  "meta": { "request_id": "df31483a-6816-4b60-9ccf-553f6b7e0687", "timestamp": "2026-09-16T18:21:12Z" }
}
```

## POST /v1/query — valid request

```bash
curl -X POST http://localhost:8080/v1/query \
  -H "Content-Type: application/json" \
  -d '{"query":"What was Q3 revenue?","session_id":"123e4567-e89b-42d3-a456-426614174000"}'
```

```json
{
  "success": true,
  "data": {
    "query_id": "f51aed29-7faa-4349-9d36-8d995db54122",
    "answer": "This is a placeholder response — RAG pipeline not yet connected.",
    "sources": [],
    "conversation_id": "8905b1c1-4a21-4055-a8de-b5182fe68fa7",
    "latency_ms": { "backend_ms": 0, "llm_ms": 0, "total_ms": 0 }
  },
  "meta": { "request_id": "d46fd432-1060-49bc-b8e1-d751d666daf9", "timestamp": "2026-09-16T18:21:26Z" }
}
```

## POST /v1/query — validation error (missing field)

```bash
curl -X POST http://localhost:8080/v1/query \
  -H "Content-Type: application/json" \
  -d '{"session_id":"123e4567-e89b-42d3-a456-426614174000"}'
```

```json
{
  "success": false,
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "'query' is required and must be a string",
    "details": { "field": "query" }
  },
  "meta": { "request_id": "9b5ef5cf-f991-4fb3-825a-4f89eff76984", "timestamp": "2026-09-16T18:21:26Z" }
}
```

## POST /v1/query — unknown field (strict schema)

```bash
curl -X POST http://localhost:8080/v1/query \
  -H "Content-Type: application/json" \
  -d '{"query":"hi","session_id":"123e4567-e89b-42d3-a456-426614174000","hack":true}'
```

```json
{
  "success": false,
  "error": { "code": "VALIDATION_ERROR", "message": "Unknown field: hack", "details": { "field": "hack" } },
  "meta": { "request_id": "3ccf36c3-8058-4894-ac3a-5e8c2a69ea31", "timestamp": "2026-09-16T18:21:26Z" }
}
```

## POST /v1/data/upload — valid file

```bash
curl -X POST http://localhost:8080/v1/data/upload \
  -F "file=@report.pdf" -F "file_type=pdf"
```

```json
{
  "success": true,
  "data": {
    "document_id": "de3265c7-05d9-488a-b290-aaa1ecf36a07",
    "status": "queued",
    "filename": "report.pdf",
    "size_bytes": 35,
    "uploaded_at": "2026-09-16T18:21:26Z"
  },
  "meta": { "request_id": "57a673c1-1f8c-4788-89f8-180c8fcaa2a8", "timestamp": "2026-09-16T18:21:26Z" }
}
```

Response status code: `201 Created`.

## POST /v1/data/upload — unsupported file_type

```bash
curl -X POST http://localhost:8080/v1/data/upload \
  -F "file=@report.pdf" -F "file_type=exe"
```

```json
{
  "success": false,
  "error": {
    "code": "UNSUPPORTED_FILE_TYPE",
    "message": "Unsupported file_type: exe",
    "details": { "allowed": ["pdf", "csv", "xlsx", "json"] }
  },
  "meta": { "request_id": "7168cc39-a038-4a55-bbb2-f9699c89126e", "timestamp": "2026-09-16T18:21:26Z" }
}
```

Response status code: `415 Unsupported Media Type`.

## GET /v1/unknown-route — 404

```bash
curl http://localhost:8080/v1/nope
```

```json
{
  "success": false,
  "error": { "code": "NOT_FOUND", "message": "Route not found", "details": {} },
  "meta": { "request_id": "16a168b0-90d3-4fd1-8d23-6af30f05b0d6", "timestamp": "2026-09-16T18:21:24Z" }
}
```

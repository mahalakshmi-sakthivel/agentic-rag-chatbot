#!/usr/bin/env bash
# examples/smoke_test.sh
#
# Live HTTP verification of Phase 1 endpoints AND their response envelopes
# against a running server — not just HTTP status codes. Complements the
# GoogleTest unit tests (which test pure logic, not live HTTP).
#
# Requires: curl, jq
# Usage: ./smoke_test.sh [base_url]   (defaults to http://localhost:8080)

set -euo pipefail
BASE_URL="${1:-http://localhost:8080}"
PASS=0
FAIL=0

# check_status: HTTP status code only
check_status() {
  local desc="$1" expected="$2" actual="$3"
  if [ "$actual" = "$expected" ]; then
    echo "  OK   $desc (status $actual)"
    PASS=$((PASS + 1))
  else
    echo "  FAIL $desc (expected status $expected, got $actual)"
    FAIL=$((FAIL + 1))
  fi
}

# check_json: a jq filter against the last response body must equal a value.
# This is what actually verifies the ENVELOPE (Section 7.3), not just status.
check_json() {
  local desc="$1" filter="$2" expected="$3"
  local actual
  actual=$(jq -r "$filter" /tmp/resp.json 2>/dev/null || echo "<jq error>")
  if [ "$actual" = "$expected" ]; then
    echo "  OK   $desc"
    PASS=$((PASS + 1))
  else
    echo "  FAIL $desc (filter '$filter' expected '$expected', got '$actual')"
    FAIL=$((FAIL + 1))
  fi
}

# check_json_present: a jq filter must exist / be non-null (e.g. request_id).
check_json_present() {
  local desc="$1" filter="$2"
  local actual
  actual=$(jq -r "$filter" /tmp/resp.json 2>/dev/null || echo "null")
  if [ "$actual" != "null" ] && [ -n "$actual" ]; then
    echo "  OK   $desc (got '$actual')"
    PASS=$((PASS + 1))
  else
    echo "  FAIL $desc (filter '$filter' was missing/null)"
    FAIL=$((FAIL + 1))
  fi
}

echo "== GET /v1/health =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" "$BASE_URL/v1/health")
check_status "health returns 200" 200 "$status"
check_json "envelope: success=true"        '.success' 'true'
check_json "data.status is ok"             '.data.status' 'ok'
check_json_present "meta.request_id present" '.meta.request_id'
check_json_present "meta.timestamp present"   '.meta.timestamp'

echo
echo "== POST /v1/query — valid =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{"query":"What was Q3 revenue?","session_id":"123e4567-e89b-42d3-a456-426614174000"}')
check_status "valid query returns 200" 200 "$status"
check_json "envelope: success=true"        '.success' 'true'
check_json "sources is an empty array"     '.data.sources | length' '0'
check_json "latency_ms.llm_ms is 0 (stub)" '.data.latency_ms.llm_ms' '0'
check_json_present "data.query_id present"        '.data.query_id'
check_json_present "data.conversation_id present" '.data.conversation_id'

echo
echo "== POST /v1/query — missing query field =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"123e4567-e89b-42d3-a456-426614174000"}')
check_status "missing query returns 400" 400 "$status"
check_json "envelope: success=false"          '.success' 'false'
check_json "error.code is VALIDATION_ERROR"   '.error.code' 'VALIDATION_ERROR'
check_json "error.details.field is 'query'"   '.error.details.field' 'query'

echo
echo "== POST /v1/query — unknown field (strict schema) =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{"query":"hi","session_id":"123e4567-e89b-42d3-a456-426614174000","hack":true}')
check_status "unknown field returns 400" 400 "$status"
check_json "error.code is VALIDATION_ERROR" '.error.code' 'VALIDATION_ERROR'
check_json "error.details.field is 'hack'"  '.error.details.field' 'hack'

echo
echo "== POST /v1/query — malformed JSON =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{not valid json')
check_status "malformed JSON returns 400" 400 "$status"
check_json "error.code is VALIDATION_ERROR" '.error.code' 'VALIDATION_ERROR'

echo
echo "== GET unknown route =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" "$BASE_URL/v1/nope")
check_status "unknown route returns 404" 404 "$status"
check_json "error.code is NOT_FOUND" '.error.code' 'NOT_FOUND'

echo
echo "== POST /v1/data/upload — valid pdf =="
echo "%PDF-1.4 fake pdf content for smoke test" > /tmp/smoke_test.pdf
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/data/upload" \
  -F "file=@/tmp/smoke_test.pdf" -F "file_type=pdf")
check_status "valid upload returns 201" 201 "$status"
check_json "envelope: success=true"    '.success' 'true'
check_json "status is queued"          '.data.status' 'queued'
check_json_present "document_id present" '.data.document_id'

echo
echo "== POST /v1/data/upload — unsupported file_type =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/data/upload" \
  -F "file=@/tmp/smoke_test.pdf" -F "file_type=exe")
check_status "unsupported file_type returns 415" 415 "$status"
check_json "error.code is UNSUPPORTED_FILE_TYPE" '.error.code' 'UNSUPPORTED_FILE_TYPE'

echo
echo "== POST /v1/data/upload — path traversal filename =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/data/upload" \
  -F "file=@/tmp/smoke_test.pdf;filename=../../etc/passwd" -F "file_type=pdf")
check_status "path traversal upload still returns 201 (sanitized, not rejected)" 201 "$status"
check_json "filename was sanitized to 'passwd'" '.data.filename' 'passwd'

echo
echo "== Summary: $PASS passed, $FAIL failed =="
[ "$FAIL" -eq 0 ]

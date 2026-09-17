#!/usr/bin/env bash
# examples/smoke_test.sh
#
# Live HTTP verification of all Phase 1 endpoints against a running server.
# Complements the GoogleTest unit tests (which test pure logic, not live
# HTTP). Run this after starting ./agentic_rag_backend.
#
# Usage: ./smoke_test.sh [base_url]   (defaults to http://localhost:8080)

set -euo pipefail
BASE_URL="${1:-http://localhost:8080}"
PASS=0
FAIL=0

check() {
  local desc="$1" expected_status="$2" actual_status="$3"
  if [ "$actual_status" = "$expected_status" ]; then
    echo "  OK   $desc (got $actual_status)"
    PASS=$((PASS + 1))
  else
    echo "  FAIL $desc (expected $expected_status, got $actual_status)"
    FAIL=$((FAIL + 1))
  fi
}

echo "== /v1/health =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" "$BASE_URL/v1/health")
check "health returns 200" 200 "$status"
cat /tmp/resp.json; echo

echo "== POST /v1/query — valid =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{"query":"What was Q3 revenue?","session_id":"123e4567-e89b-42d3-a456-426614174000"}')
check "valid query returns 200" 200 "$status"
cat /tmp/resp.json; echo

echo "== POST /v1/query — missing query field =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"123e4567-e89b-42d3-a456-426614174000"}')
check "missing query returns 400" 400 "$status"

echo "== POST /v1/query — unknown field =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{"query":"hi","session_id":"123e4567-e89b-42d3-a456-426614174000","hack":true}')
check "unknown field returns 400" 400 "$status"

echo "== POST /v1/query — malformed JSON =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/query" \
  -H "Content-Type: application/json" \
  -d '{not valid json')
check "malformed JSON returns 400" 400 "$status"

echo "== GET unknown route =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" "$BASE_URL/v1/nope")
check "unknown route returns 404" 404 "$status"

echo "== POST /v1/data/upload — valid pdf =="
echo "%PDF-1.4 fake pdf content for smoke test" > /tmp/smoke_test.pdf
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/data/upload" \
  -F "file=@/tmp/smoke_test.pdf" -F "file_type=pdf")
check "valid upload returns 201" 201 "$status"
cat /tmp/resp.json; echo

echo "== POST /v1/data/upload — unsupported file_type =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/data/upload" \
  -F "file=@/tmp/smoke_test.pdf" -F "file_type=exe")
check "unsupported file_type returns 415" 415 "$status"

echo "== POST /v1/data/upload — path traversal filename =="
status=$(curl -sS -o /tmp/resp.json -w "%{http_code}" -X POST "$BASE_URL/v1/data/upload" \
  -F "file=@/tmp/smoke_test.pdf;filename=../../etc/passwd" -F "file_type=pdf")
check "path traversal upload still returns 201 (sanitized)" 201 "$status"
if grep -q '"filename":"passwd"' /tmp/resp.json; then
  echo "  OK   filename was sanitized to 'passwd'"
  PASS=$((PASS + 1))
else
  echo "  FAIL filename was NOT sanitized as expected"
  FAIL=$((FAIL + 1))
fi

echo
echo "== Summary: $PASS passed, $FAIL failed =="
[ "$FAIL" -eq 0 ]

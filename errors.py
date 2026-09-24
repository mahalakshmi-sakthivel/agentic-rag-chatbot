# app/errors.py
#
# Error taxonomy for Phase 6, matching PHASE_5 spec §19A.5 exactly (Phase 5's
# client already parses these codes/status combinations, see
# app/clients/phase6_client.py in the Phase 5 repo). Do not add codes or
# change status mappings without updating that table AND raising a
# TECHNICAL_CONTRACT.md Section 27 amendment (§19A is still [PROPOSED]).
from datetime import datetime, timezone
from typing import Any, Dict, Optional
from uuid import uuid4

from fastapi.responses import JSONResponse


class Phase6Exception(Exception):
    def __init__(
        self,
        code: str,
        message: str,
        status_code: int,
        details: Optional[Dict[str, Any]] = None,
    ):
        super().__init__(message)
        self.code = code
        self.message = message
        self.status_code = status_code
        self.details = details or {}


class ValidationError(Phase6Exception):
    """Request body did not match OrchestratorContext. §19A.5: 400, no retry,
    Phase 5 treats this as an integration defect (not shown to the end user)."""

    def __init__(self, message: str, details: Optional[Dict[str, Any]] = None):
        super().__init__("VALIDATION_ERROR", message, 400, details)


class UnauthenticatedError(Phase6Exception):
    """Missing/invalid X-Internal-Auth. §19A.5: 401, no retry, Phase 5 logs
    and alerts (this should never happen in normal operation)."""

    def __init__(self, message: str = "Missing or invalid internal service token"):
        super().__init__("UNAUTHENTICATED", message, 401)


class LlmRateLimitError(Phase6Exception):
    def __init__(self, message: str = "LLM provider rate-limited this request"):
        super().__init__("LLM_RATE_LIMIT", message, 429)


class LlmProviderError(Phase6Exception):
    def __init__(self, message: str = "LLM provider request failed"):
        super().__init__("LLM_PROVIDER_ERROR", message, 502)


class LlmTimeoutError(Phase6Exception):
    def __init__(self, message: str = "LLM provider request timed out"):
        super().__init__("LLM_TIMEOUT", message, 504)


class LlmContentFilteredError(Phase6Exception):
    """HTTP status is genuinely TBD per §19A.5 ('Content filtered | ... | TBD').
    422 is used here as a reasoned default (syntactically valid request,
    semantically rejected content) — raise the final choice via Section 27."""

    def __init__(self, message: str = "LLM provider declined to generate a response"):
        super().__init__("LLM_CONTENT_FILTERED", message, 422)


class InternalError(Phase6Exception):
    def __init__(self, message: str = "Internal server error"):
        super().__init__("INTERNAL_ERROR", message, 500)


def format_error_payload(
    code: str,
    message: str,
    details: Optional[Dict[str, Any]] = None,
    request_id: Optional[str] = None,
) -> Dict[str, Any]:
    return {
        "success": False,
        "error": {"code": code, "message": message, "details": details or {}},
        "meta": {
            "request_id": request_id or str(uuid4()),
            "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        },
    }


def build_error_response(
    code: str,
    message: str,
    status_code: int,
    details: Optional[Dict[str, Any]] = None,
    request_id: Optional[str] = None,
) -> JSONResponse:
    payload = format_error_payload(code, message, details, request_id)
    return JSONResponse(status_code=status_code, content=payload)

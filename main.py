# app/main.py
#
# Phase 6 — LLM Integration & Response Generation.
#
# Exposes exactly one internal endpoint, per Phase 5 spec §19A.1:
#
#     POST /internal/v1/llm/generate
#
# Caller: Phase 5 only, over localhost / a private network. Never exposed
# through Phase 1 or reachable by the host application/UI (§19A.6).
from __future__ import annotations

import logging
from datetime import datetime, timezone
from uuid import uuid4

from fastapi import Depends, FastAPI, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from .auth import require_internal_auth
from .errors import Phase6Exception, build_error_response
from .llm_client import LLMClient, get_llm_client
from .schemas import OrchestratorContext, Phase6SuccessResponse
from .services import generate_answer

logger = logging.getLogger("phase6")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

app = FastAPI(title="Phase 6 LLM Integration & Response Generation", version="1.0.0")


# --- Exception handlers (mirrors Phase 4's envelope pattern for consistency) ---

@app.exception_handler(Phase6Exception)
async def phase6_exception_handler(request: Request, exc: Phase6Exception):
    request_id = request.headers.get("X-Request-ID")
    logger.warning(
        "phase6_error code=%s request_id=%s message=%s",
        exc.code,
        request_id,
        exc.message,
    )
    return build_error_response(
        code=exc.code,
        message=exc.message,
        status_code=exc.status_code,
        details=exc.details,
        request_id=request_id,
    )


@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    errors = exc.errors()
    msg = "; ".join(
        f"{'.'.join(str(x) for x in err.get('loc', []))}: {err.get('msg')}" for err in errors
    )
    # Pydantic puts the raw exception object in each error's `ctx` (e.g.
    # {"error": ValueError(...)}), which json.dumps cannot serialize.
    # Strip it — the human-readable `msg` already carries the same text.
    safe_errors = [{k: v for k, v in err.items() if k != "ctx"} for err in errors]
    request_id = request.headers.get("X-Request-ID")
    return build_error_response(
        code="VALIDATION_ERROR",
        message=f"Request validation failed: {msg}",
        status_code=400,
        details={"errors": safe_errors},
        request_id=request_id,
    )


@app.exception_handler(Exception)
async def generic_exception_handler(request: Request, exc: Exception):
    # Never leak stack traces, internal URLs, or tokens across the boundary
    # (spec §19A.5).
    request_id = request.headers.get("X-Request-ID")
    return build_error_response(
        code="INTERNAL_ERROR",
        message="An unexpected internal error occurred",
        status_code=500,
        request_id=request_id,
    )


# --- Routes ---

@app.get("/health")
def health():
    return {"status": "ok"}


@app.post("/internal/v1/llm/generate", response_model=Phase6SuccessResponse)
def generate(
    context: OrchestratorContext,
    request: Request,
    _: None = Depends(require_internal_auth),
    llm_client: LLMClient = Depends(get_llm_client),
):
    data = generate_answer(context, llm_client)

    request_id = request.headers.get("X-Request-ID", str(uuid4()))
    logger.info(
        "phase6_generate_complete query_id=%s request_id=%s llm_ms=%s passed=%s groundedness=%s citation_coverage=%s",
        data.query_id,
        request_id,
        data.llm_ms,
        data.validation.passed,
        data.validation.groundedness,
        data.validation.citation_coverage,
    )
    return Phase6SuccessResponse(
        success=True,
        data=data,
        meta={
            "request_id": request_id,
            "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        },
    )

# app/schemas.py
#
# Request/response schemas for POST /internal/v1/llm/generate.
#
# These field names and shapes are copied verbatim from Phase 5's actual
# implementation (app/schemas/models.py in the Phase 5 repo) and from
# PHASE_5_AGENTIC_RAG_FINAL_PYTHON_SCOPE_v2.1 §19A.3/19A.4 — they are NOT
# reinvented here. Phase 5's `phase6_client.py` calls this exact contract,
# so any change here must be coordinated with Phase 5 (and, per §19A.9,
# formally recorded as a TECHNICAL_CONTRACT.md Section 27 amendment, since
# §19A is currently marked [PROPOSED]).
from __future__ import annotations

from typing import Any, Dict, List, Optional

from pydantic import BaseModel, ConfigDict, field_validator


# ---------------------------------------------------------------------------
# Request: OrchestratorContext (Phase 5 -> Phase 6)
# ---------------------------------------------------------------------------
class Identity(BaseModel):
    user_id: str
    tenant_id: str
    roles: List[str]
    session_id: str

    @field_validator("roles")
    @classmethod
    def validate_roles(cls, roles_list: List[str]) -> List[str]:
        allowed = {"user", "admin"}
        for r in roles_list:
            if r not in allowed:
                raise ValueError(f"Invalid role '{r}'. Only 'user' and 'admin' are allowed.")
        return roles_list


class Chunk(BaseModel):
    chunk_id: str
    document_id: str
    text: str
    score: float
    source_location: Optional[str] = None
    filename: Optional[str] = None


class Step(BaseModel):
    step: str
    tool: str
    result_summary: str


class OrchestratorContext(BaseModel):
    # extra='forbid' matches Phase 5's model exactly — an unexpected field
    # from Phase 5 should fail loudly (400 VALIDATION_ERROR) rather than be
    # silently ignored, since that usually means the two sides have drifted.
    model_config = ConfigDict(extra="forbid")

    query_id: str
    identity: Identity
    original_query: str
    retrieved_chunks: List[Chunk]
    refined_query: Optional[str] = None
    steps_taken: Optional[List[Step]] = None
    conversation_history: Optional[List[Dict[str, Any]]] = None
    tool_results: Optional[List[Dict[str, Any]]] = None


# ---------------------------------------------------------------------------
# Response: Phase6Data (Phase 6 -> Phase 5), §19A.4 / contract §8.1, §17.1
# ---------------------------------------------------------------------------
class Phase6Source(BaseModel):
    document_id: str
    chunk_id: str
    excerpt: str
    score: float
    page_or_location: Optional[str] = None


class Phase6Validation(BaseModel):
    groundedness: str  # "high" | "medium" | "low"
    unsupported_claims: List[str]
    citation_coverage: float
    passed: bool


class Phase6Data(BaseModel):
    query_id: str
    answer: str
    sources: List[Phase6Source]
    validation: Phase6Validation
    llm_ms: int


class Phase6Meta(BaseModel):
    request_id: str
    timestamp: str


class Phase6SuccessResponse(BaseModel):
    success: bool = True
    data: Phase6Data
    meta: Phase6Meta


class Phase6ErrorDetail(BaseModel):
    code: str
    message: str
    details: Dict[str, Any] = {}


class Phase6ErrorResponse(BaseModel):
    success: bool = False
    error: Phase6ErrorDetail
    meta: Phase6Meta

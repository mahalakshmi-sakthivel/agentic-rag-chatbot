import pytest
import os
from pydantic import ValidationError

from app.schemas.models import (
    Identity, Chunk, Step, OrchestratorContext, Intent, EvaluationState, ErrorCode,
    Phase6SuccessResponse, Phase6ErrorResponse
)
from app.config import Config, get_config

def test_identity_frozen():
    """Spec 19: Identity (frozen)"""
    identity = Identity(user_id="user_123", tenant_id="tenant_456")
    with pytest.raises(ValidationError):
        identity.user_id = "new_user"

def test_orchestrator_context_required_fields():
    """Spec 19: OrchestratorContext required and optional fields exactly as spec 19"""
    identity = Identity(user_id="user_123", tenant_id="tenant_456")
    
    # Missing required fields
    with pytest.raises(ValidationError):
        OrchestratorContext(
            query_id="q123",
            identity=identity,
            original_query="hello"
            # Missing retrieved_chunks
        )

    # Valid with required fields
    ctx = OrchestratorContext(
        query_id="q123",
        identity=identity,
        original_query="hello",
        retrieved_chunks=[]
    )
    assert ctx.query_id == "q123"
    assert ctx.refined_query is None

def test_orchestrator_context_reject_unknown_fields():
    """Spec 19: reject unknown fields"""
    identity = Identity(user_id="user_123", tenant_id="tenant_456")
    
    with pytest.raises(ValidationError) as exc:
        OrchestratorContext(
            query_id="q123",
            identity=identity,
            original_query="hello",
            retrieved_chunks=[],
            unknown_field="should fail"
        )
    assert "Extra inputs are not permitted" in str(exc.value)

def test_config_defaults(monkeypatch):
    """Spec 18: Limits (configurable, defaults from spec 18)"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    
    config = Config()
    assert config.MAX_RETRIEVAL_ATTEMPTS == 2
    assert config.MAX_TOOL_CALLS == 5
    assert config.MAX_PLAN_STEPS == 8
    assert config.MAX_CLARIFICATION_ATTEMPTS == 1
    assert config.MAX_TOP_K == 20
    assert config.MAX_PHASE6_CALLS == 1
    
    # Assert missing required fields
    monkeypatch.delenv("INTERNAL_SERVICE_TOKEN", raising=False)
    with pytest.raises(ValueError) as exc:
        Config()
    assert "Missing required environment variable: INTERNAL_SERVICE_TOKEN" in str(exc.value)

def test_phase6_success_envelope():
    """Spec 19A.4: Phase 6 success envelopes"""
    data = {
        "success": True,
        "data": {
            "query_id": "uuid",
            "answer": "Q3 revenue was ₹12.5 lakh.",
            "sources": [
                {
                    "document_id": "uuid",
                    "chunk_id": "uuid",
                    "excerpt": "Q3 revenue was ₹12.5 lakh.",
                    "score": 0.92,
                    "page_or_location": "page 5"
                }
            ],
            "validation": {
                "groundedness": "high",
                "unsupported_claims": [],
                "citation_coverage": 0.92,
                "passed": True
            },
            "llm_ms": 950
        },
        "meta": {
            "request_id": "uuid",
            "timestamp": "2026-09-20T10:00:00Z"
        }
    }
    resp = Phase6SuccessResponse(**data)
    assert resp.success is True
    assert resp.data.llm_ms == 950

def test_phase6_error_envelope():
    """Spec 19A.5: Phase 6 error envelopes"""
    data = {
      "success": False,
      "error": { "code": "LLM_TIMEOUT", "message": "Human-readable message", "details": {} },
      "meta": { "request_id": "uuid", "timestamp": "2026-09-20T10:00:00Z" }
    }
    resp = Phase6ErrorResponse(**data)
    assert resp.success is False
    assert resp.error.code == "LLM_TIMEOUT"

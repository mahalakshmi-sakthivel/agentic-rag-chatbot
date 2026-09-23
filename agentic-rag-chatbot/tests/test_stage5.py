import pytest
import httpx
from app.clients.phase6_client import call_phase6, Phase6HandoffError, reset_httpx_client
from app.schemas.models import OrchestratorContext, Identity, Chunk, ErrorCode
from app.config import clear_config

@pytest.fixture(autouse=True)
def setup_teardown():
    clear_config()
    reset_httpx_client()
    yield
    clear_config()
    reset_httpx_client()

def get_valid_context():
    return OrchestratorContext(
        query_id="test-query-id",
        identity=Identity(user_id="u1", tenant_id="t1", roles=["user"], session_id="s1"),
        original_query="test",
        retrieved_chunks=[Chunk(chunk_id="1", document_id="1", text="text", score=0.9, source_location="1", filename="1.pdf")]
    )

def test_phase6_success(monkeypatch, httpx_mock):
    """Spec 19A.4: Success response"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    
    context = get_valid_context()
    
    mock_resp = {
        "success": True,
        "data": {
            "query_id": "test-query-id",
            "answer": "Answer",
            "sources": [],
            "validation": {"groundedness": "high", "unsupported_claims": [], "citation_coverage": 1.0, "passed": True},
            "llm_ms": 100
        },
        "meta": {"request_id": "uuid", "timestamp": "2026"}
    }
    httpx_mock.add_response(json=mock_resp)
    
    res = call_phase6(context)
    assert res.answer == "Answer"

@pytest.mark.parametrize("status, error_code, expected_mapped_code", [
    (400, "VALIDATION_ERROR", ErrorCode.INTERNAL_ERROR),
    (401, "UNAUTHENTICATED", ErrorCode.INTERNAL_ERROR),
    (429, "LLM_RATE_LIMIT", ErrorCode.LLM_RATE_LIMIT),
    (502, "LLM_PROVIDER_ERROR", ErrorCode.LLM_PROVIDER_ERROR),
    (504, "LLM_TIMEOUT", ErrorCode.LLM_TIMEOUT),
    (500, "INTERNAL_ERROR", ErrorCode.INTERNAL_ERROR),
    (418, "LLM_CONTENT_FILTERED", ErrorCode.LLM_CONTENT_FILTERED) 
])
def test_phase6_error_mapping(monkeypatch, httpx_mock, status, error_code, expected_mapped_code):
    """Spec 19A.5: Error mapping exactly per table."""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    
    mock_resp = {
        "success": False,
        "error": {"code": error_code, "message": "msg", "details": {}},
        "meta": {"request_id": "uuid", "timestamp": "2026"}
    }
    httpx_mock.add_response(status_code=status, json=mock_resp)
    
    with pytest.raises(Phase6HandoffError) as exc:
        call_phase6(get_valid_context())
        
    assert exc.value.code == expected_mapped_code
    assert len(httpx_mock.get_requests()) == 1 # No retry

def test_phase6_client_timeout(monkeypatch, httpx_mock):
    """Spec 19A.5: Client timeout -> LLM_TIMEOUT (no retry)"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    
    httpx_mock.add_exception(httpx.ReadTimeout("Timeout"))
    
    with pytest.raises(Phase6HandoffError) as exc:
        call_phase6(get_valid_context())
        
    assert exc.value.code == ErrorCode.LLM_TIMEOUT
    assert len(httpx_mock.get_requests()) == 1 # No retry

def test_phase6_connection_error_retry(monkeypatch, httpx_mock):
    """Spec 19A.5: Phase 6 unreachable -> Retry once, then return INTERNAL_ERROR"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    monkeypatch.setenv("PHASE6_MAX_RETRIES", "1")
    
    # Needs to fail both times
    httpx_mock.add_exception(httpx.ConnectError("Connection refused"))
    httpx_mock.add_exception(httpx.ConnectError("Connection refused"))
    
    with pytest.raises(Phase6HandoffError) as exc:
        call_phase6(get_valid_context())
        
    assert exc.value.code == ErrorCode.INTERNAL_ERROR
    assert len(httpx_mock.get_requests()) == 2 # 1 initial + 1 retry

def test_scenario_7_handoff_failure(monkeypatch, httpx_mock):
    """Spec 27: Scenario 7 — Phase 6 handoff failure (integration in agent.py)"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    
    from app.agent.agent import Agent
    from app.tools.registry import ToolRegistry
    from app.tools.vector_search import VectorSearchTool
    from app.clients.phase4_client import MockPhase4Client
    from app.schemas.models import AgentRequest

    registry = ToolRegistry()
    registry.register(VectorSearchTool(MockPhase4Client()))
    agent = Agent(registry)
    
    # 504 LLM_TIMEOUT response
    mock_resp = {
        "success": False,
        "error": {"code": "LLM_TIMEOUT", "message": "msg", "details": {}},
        "meta": {"request_id": "uuid", "timestamp": "2026"}
    }
    httpx_mock.add_response(status_code=504, json=mock_resp)
    
    request = AgentRequest(query="test", identity=Identity(user_id="u1", tenant_id="t1", roles=["user"], session_id="s1"), query_id="my-query-id")
    result = agent.run(request)
    
    # Contract-compatible failure returned upward (query_id preserved)
    from app.schemas.models import FailureResponse
    assert isinstance(result, FailureResponse)
    assert result.error_code == ErrorCode.LLM_TIMEOUT
    assert result.query_id == "my-query-id"

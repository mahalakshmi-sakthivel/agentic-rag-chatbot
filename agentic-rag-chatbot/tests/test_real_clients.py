import pytest
import httpx
from app.clients.phase4_client import RealPhase4Client, Phase4Request, Phase4HandoffError
from app.clients.phase6_client import call_phase6, Phase6HandoffError
from app.schemas.models import OrchestratorContext, Identity, ErrorCode
from app.config import clear_config

@pytest.fixture(autouse=True)
def reset_config(monkeypatch):
    clear_config()
    monkeypatch.setenv("PHASE4_BASE_URL", "http://localhost:8004")
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    yield
    clear_config()

def test_real_phase4_client_success(httpx_mock):
    """Test RealPhase4Client successfully parsing Phase 4 output."""
    httpx_mock.add_response(
        url="http://localhost:8004/v1/retrieval/search",
        json={
            "results": [{
                "chunk_id": "c1", "document_id": "d1", "text": "text1",
                "score": 0.9, "source_location": "p1", "filename": "f1"
            }]
        },
        status_code=200
    )
    
    client = RealPhase4Client()
    req = Phase4Request(query_text="q", tenant_id="t1", user_id="u1", top_k=5)
    resp = client.search(req)
    
    assert len(resp.results) == 1
    assert resp.results[0].chunk_id == "c1"
    
    # Check Auth Propagation
    request_made = httpx_mock.get_request()
    assert request_made.headers["X-Internal-Auth"] == "secret-token"

def test_real_phase4_client_timeout(httpx_mock):
    """Test RealPhase4Client handles timeouts."""
    httpx_mock.add_exception(httpx.ReadTimeout("Timeout"), url="http://localhost:8004/v1/retrieval/search")
    
    client = RealPhase4Client()
    req = Phase4Request(query_text="q", tenant_id="t1", user_id="u1", top_k=5)
    
    with pytest.raises(Phase4HandoffError) as exc:
        client.search(req)
    assert exc.value.code == ErrorCode.RETRIEVAL_TIMEOUT

def test_real_phase4_client_error(httpx_mock):
    """Test RealPhase4Client handles HTTP errors."""
    httpx_mock.add_response(url="http://localhost:8004/v1/retrieval/search", status_code=401)
    
    client = RealPhase4Client()
    req = Phase4Request(query_text="q", tenant_id="t1", user_id="u1", top_k=5)
    with pytest.raises(Phase4HandoffError) as exc:
        client.search(req)
    assert exc.value.code == ErrorCode.UNAUTHENTICATED

def test_real_phase6_client_success(httpx_mock):
    """Test Phase 6 client success and auth propagation."""
    httpx_mock.add_response(
        url="http://localhost:8006/internal/v1/llm/generate",
        json={
            "success": True,
            "data": {
                "query_id": "q1",
                "answer": "Phase 6 answer",
                "citations": [],
                "sources": [],
                "validation": {
                    "groundedness": "high",
                    "unsupported_claims": [],
                    "citation_coverage": 1.0,
                    "passed": True
                },
                "llm_ms": 100
            },
            "meta": {
                "request_id": "req-1",
                "timestamp": "2023-01-01T00:00:00Z"
            }
        },
        status_code=200
    )
    
    ctx = OrchestratorContext(
        query_id="q1",
        identity=Identity(user_id="u1", tenant_id="t1"),
        original_query="q",
        retrieved_chunks=[]
    )
    
    result = call_phase6(ctx)
    assert result.answer == "Phase 6 answer"
    
    request_made = httpx_mock.get_request()
    assert request_made.headers["X-Internal-Auth"] == "secret-token"

def test_real_phase6_retry(httpx_mock):
    """Test Phase 6 retry on connection error."""
    # First fail with ConnectError, then succeed
    httpx_mock.add_exception(httpx.ConnectError("Conn error"))
    httpx_mock.add_response(
        json={
            "success": True,
            "data": {
                "query_id": "q1",
                "answer": "Retry success",
                "citations": [],
                "sources": [],
                "validation": {
                    "groundedness": "high",
                    "unsupported_claims": [],
                    "citation_coverage": 1.0,
                    "passed": True
                },
                "llm_ms": 100
            },
            "meta": {
                "request_id": "req-1",
                "timestamp": "2023-01-01T00:00:00Z"
            }
        },
        status_code=200
    )
    
    ctx = OrchestratorContext(
        query_id="q1",
        identity=Identity(user_id="u1", tenant_id="t1"),
        original_query="q",
        retrieved_chunks=[]
    )
    
    result = call_phase6(ctx)
    assert result.answer == "Retry success"
    assert len(httpx_mock.get_requests()) == 2

def test_real_phase6_timeout(httpx_mock):
    """Test Phase 6 timeout doesn't retry."""
    httpx_mock.add_exception(httpx.ReadTimeout("Timeout"))
    ctx = OrchestratorContext(
        query_id="q1",
        identity=Identity(user_id="u1", tenant_id="t1"),
        original_query="q",
        retrieved_chunks=[]
    )
    
    with pytest.raises(Phase6HandoffError) as exc:
        call_phase6(ctx)
    assert exc.value.code == ErrorCode.LLM_TIMEOUT
    assert len(httpx_mock.get_requests()) == 1

def test_real_phase6_malformed_response(httpx_mock):
    """Test Phase 6 handles malformed json responses."""
    httpx_mock.add_response(
        url="http://localhost:8006/internal/v1/llm/generate",
        text="Not JSON",
        status_code=500
    )
    
    ctx = OrchestratorContext(
        query_id="q1",
        identity=Identity(user_id="u1", tenant_id="t1"),
        original_query="q",
        retrieved_chunks=[]
    )
    
    with pytest.raises(Phase6HandoffError) as exc:
        call_phase6(ctx)
    assert exc.value.code == ErrorCode.INTERNAL_ERROR

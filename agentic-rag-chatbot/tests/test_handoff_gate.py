from app.schemas.models import AgentRequest, Identity, ClarificationResponse, FailureResponse, ErrorCode
from app.agent.agent import Agent
from app.tools.registry import ToolRegistry
from app.tools.vector_search import VectorSearchTool
from app.clients.phase4_client import MockPhase4Client

def test_handoff_gate_no_call_on_clarification_or_empty(monkeypatch, httpx_mock):
    """Spec 19A.2: Handoff Gate prevents Phase 6 calls on clarification or empty evidence."""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    
    registry = ToolRegistry()
    registry.register(VectorSearchTool(MockPhase4Client()))
    agent = Agent(registry)
    
    # 1. Clarification (no call)
    req1 = AgentRequest(query="What was the revenue?", identity=Identity(user_id="u1", tenant_id="t1"))
    res1 = agent.run(req1)
    assert isinstance(res1, ClarificationResponse)
    assert len(httpx_mock.get_requests()) == 0
    
    # 2. Empty Evidence (no call)
    # A query that doesn't trigger mock client to return anything.
    # The mock returns chunk only if text is provided. Wait, our mock returns chunk always if query_text is given.
    # We can mock it to return empty.
    class EmptyPhase4Client:
        def search(self, request):
            from app.clients.phase4_client import Phase4Response
            return Phase4Response(results=[])
            
    registry = ToolRegistry()
    registry.register(VectorSearchTool(EmptyPhase4Client()))
    agent = Agent(registry)
    
    req2 = AgentRequest(query="find empty things", identity=Identity(user_id="u1", tenant_id="t1"))
    res2 = agent.run(req2)
    assert isinstance(res2, FailureResponse)
    assert res2.error_code == ErrorCode.INSUFFICIENT_CONTEXT
    assert len(httpx_mock.get_requests()) == 0

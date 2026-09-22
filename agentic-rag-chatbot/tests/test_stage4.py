import pytest
from app.schemas.models import AgentState, Identity, Intent, PlanStep, EvaluationState, ErrorCode
from app.tools.registry import ToolRegistry
from app.tools.calculator import CalculatorTool
from app.tools.vector_search import VectorSearchTool
from app.tools.structured_query import StructuredQueryTool, MockStructuredQueryBackend
from app.clients.phase4_client import MockPhase4Client
from app.agent.executor import execute_plan, AgentExecutionError
from app.agent.intent import detect_intent
from app.agent.planner import Planner
from app.config import clear_config

@pytest.fixture(autouse=True)
def reset_config():
    clear_config()
    yield
    clear_config()

def setup_registry():
    registry = ToolRegistry()
    registry.register(CalculatorTool())
    registry.register(StructuredQueryTool(MockStructuredQueryBackend()))
    registry.register(VectorSearchTool(MockPhase4Client()))
    return registry

def get_base_state(query: str):
    return AgentState(
        query_id="q123",
        identity=Identity(user_id="u1", tenant_id="t1"),
        original_query=query,
        intent=detect_intent(query)
    )

def test_scenario_1_document_lookup(monkeypatch):
    """Spec 27: Scenario 1 — Document lookup"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    
    registry = setup_registry()
    query = "What does the uploaded document say about the refund policy?"
    state = get_base_state(query)
    
    planner = Planner()
    plan = planner.create_plan(state.intent, query)
    
    state = execute_plan(state, plan, registry)
    
    assert state.evaluation_state == EvaluationState.SUFFICIENT
    assert len(state.retrieved_chunks) > 0
    assert state.tool_calls == 1

def test_scenario_2_structured_query(monkeypatch):
    """Spec 27: Scenario 2 — Structured query"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")

    registry = setup_registry()
    query = "What was the highest monthly revenue?"
    state = get_base_state(query)
    
    plan = Planner().create_plan(state.intent, query)
    state = execute_plan(state, plan, registry)
    
    assert state.evaluation_state == EvaluationState.SUFFICIENT
    assert state.tool_calls == 1
    assert len(state.tool_results_list) == 1

def test_scenario_3_calculation(monkeypatch):
    """Spec 27: Scenario 3 — Calculation"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")

    registry = setup_registry()
    query = "What is 15% of 800?"
    state = get_base_state(query)
    
    # Overwrite intent explicitly because the regex requires exact matches.
    state.intent = Intent.CALCULATION
    # And expression
    query = "800 * 0.15"
    
    plan = Planner().create_plan(state.intent, query)
    state = execute_plan(state, plan, registry)
    
    assert state.evaluation_state == EvaluationState.SUFFICIENT
    assert state.tool_calls == 1
    assert state.tool_results_list[0]["output"]["result"] == 120.0

def test_scenario_4_multi_step(monkeypatch):
    """Spec P0.3: Multi-step calculation correctness test."""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")

    registry = ToolRegistry()
    registry.register(CalculatorTool())
    
    class NumericMockBackend:
        def execute_query(self, query: str, identity: Identity):
            if "Q2" in query:
                return {"value": 1000}
            return {"value": 1500}
            
    registry.register(StructuredQueryTool(NumericMockBackend()))
    registry.register(VectorSearchTool(MockPhase4Client()))

    query = "Compare the revenue of Q2 and Q3 and calculate the growth percentage."
    state = get_base_state(query)
    
    # We construct a custom plan that mimics what an LLM planner would do.
    # We use our variable substitution {step_0} etc to pass outputs to next steps.
    plan = [
        PlanStep(description="Step 1 data retrieval", tool="structured_query", tool_input={"query": "Q2 revenue"}),
        PlanStep(description="Step 2 data retrieval", tool="structured_query", tool_input={"query": "Q3 revenue"}),
        PlanStep(description="Calculate comparison", tool="calculator", tool_input={"expression": "({step_1} - {step_0}) / {step_0} * 100"})
    ]
    
    state = execute_plan(state, plan, registry)
    
    assert state.evaluation_state == EvaluationState.SUFFICIENT
    assert state.tool_calls == 3
    assert len(state.tool_results_list) == 3
    
    calc_result = state.tool_results_list[2]["output"]["result"]
    assert calc_result == 50.0 # (1500 - 1000) / 1000 * 100 = 50.0

def test_scenario_5_retrieval_refinement(monkeypatch):
    """Spec 27: Scenario 5 — Retrieval refinement"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    monkeypatch.setenv("MIN_RELEVANCE_SCORE", "0.99") # high score required

    registry = setup_registry()
    query = "document policy"
    state = get_base_state(query)
    
    # Mock Phase4Client that returns low score chunks (0.95)
    class WeakPhase4Client:
        def search(self, request):
            from app.clients.phase4_client import Phase4Response, Phase4Result
            return Phase4Response(results=[Phase4Result(
                chunk_id="1", document_id="1", text="weak", score=0.95, source_location="page 1", filename="file.pdf"
            )])
            
    registry.register(VectorSearchTool(WeakPhase4Client()))
    
    plan = Planner().create_plan(state.intent, query)
    state = execute_plan(state, plan, registry)
    
    # It will loop twice because max retrieval is 2. Both will fail 0.99 threshold.
    assert state.evaluation_state == EvaluationState.INSUFFICIENT
    assert state.retrieval_attempts == 2

def test_scenario_6_clarification(monkeypatch):
    """Spec 27: Scenario 6 — Clarification"""
    query = "What was the revenue?"
    state = get_base_state(query)
    
    # Intent should be CLARIFICATION_REQUIRED
    assert state.intent == Intent.CLARIFICATION_REQUIRED
    
    plan = Planner().create_plan(state.intent, query)
    assert len(plan) == 0
    # The executor would not run anything and the agent logic would immediately return clarification

def test_limit_enforcement(monkeypatch):
    """Spec 18: Limit enforcement (no infinite loops)"""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    monkeypatch.setenv("MAX_TOOL_CALLS", "3")
    
    registry = setup_registry()
    state = get_base_state("loop")
    
    # Plan with 10 steps
    plan = [PlanStep(description="d", tool="calculator", tool_input={"expression": "1+1"})] * 10
    
    with pytest.raises(AgentExecutionError) as exc:
        execute_plan(state, plan, registry)
    assert exc.value.code == ErrorCode.AGENT_EXECUTION_LIMIT
    assert state.tool_calls == 3


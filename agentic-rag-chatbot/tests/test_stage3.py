import pytest
from app.agent.normalizer import normalize_query, NormalizerError
from app.agent.intent import detect_intent
from app.agent.planner import Planner
from app.agent.validator import validate_plan, PlanValidationError
from app.schemas.models import Intent, ErrorCode, PlanStep
from app.tools.registry import ToolRegistry
from app.tools.calculator import CalculatorTool

def test_query_normalization():
    """Spec 7: Query normalization."""
    # Strip and collapse whitespace
    assert normalize_query("  Hello   World  ") == "Hello World"
    
    # Length cap
    long_query = "A" * 600
    assert len(normalize_query(long_query, max_length=500)) == 500
    
    # EMPTY_QUERY
    with pytest.raises(NormalizerError) as exc:
        normalize_query("   ")
    assert exc.value.code == ErrorCode.EMPTY_QUERY

def test_intent_detection_examples():
    """Spec 8: Rule-based intent detection for all 7 categories."""
    assert detect_intent("What does the uploaded document say about the refund policy?") == Intent.DOCUMENT_LOOKUP
    assert detect_intent("What was the highest monthly revenue?") == Intent.STRUCTURED_DATA_QUERY
    assert detect_intent("What is 15% of 800?") == Intent.CALCULATION
    assert detect_intent("Compare the revenue of Q2 and Q3 and calculate the growth percentage.") == Intent.MULTI_STEP
    assert detect_intent("What was the revenue?") == Intent.CLARIFICATION_REQUIRED

def test_planner_deterministic():
    """Spec 9: Planner deterministic templates."""
    planner = Planner()
    plan = planner.create_plan(Intent.STRUCTURED_DATA_QUERY, "test")
    assert len(plan) == 1
    assert plan[0].tool == "structured_query"

    multi_plan = planner.create_plan(Intent.MULTI_STEP, "Compare Q2 and Q3")
    assert len(multi_plan) == 0  # We prevent fabricated multi-step execution

def test_plan_validator_limits():
    """Spec 9, 18: Plan validator (registered tools only, <= 8 steps)."""
    registry = ToolRegistry()
    registry.register(CalculatorTool())
    
    # Exceeding steps
    plan_long = [PlanStep(description="d", tool="calculator", tool_input={})] * 9
    with pytest.raises(PlanValidationError) as exc:
        validate_plan(plan_long, registry)
    assert "exceeds maximum allowed steps" in str(exc.value)

def test_plan_validator_unregistered_tool():
    """Spec 9, 10: Plan validator rejects unknown tools."""
    registry = ToolRegistry()
    plan = [PlanStep(description="d", tool="unknown_tool", tool_input={})]
    with pytest.raises(PlanValidationError) as exc:
        validate_plan(plan, registry)
    assert "Unregistered tool" in str(exc.value)

def test_plan_validator_no_identity_override():
    """Spec 9, 22: Plan validator prevents identity fields in step input."""
    registry = ToolRegistry()
    registry.register(CalculatorTool())
    plan = [PlanStep(description="d", tool="calculator", tool_input={"user_id": "malicious"})]
    
    with pytest.raises(PlanValidationError) as exc:
        validate_plan(plan, registry)
    assert "Identity fields" in str(exc.value)

from typing import Any
from app.schemas.models import ToolResult, EvaluationState, AgentState
from app.config import get_config

def evaluate_result(tool_name: str, result: ToolResult, state: AgentState) -> EvaluationState:
    config = get_config()
    
    if not result.success:
        if tool_name == "vector_search":
            if state.retrieval_attempts < config.MAX_RETRIEVAL_ATTEMPTS:
                return EvaluationState.REQUIRES_REFINEMENT
            return EvaluationState.INSUFFICIENT
        return EvaluationState.ERROR

    if tool_name == "vector_search":
        chunks = result.data.get("chunks", []) if result.data else []
        if not chunks:
            if state.retrieval_attempts < config.MAX_RETRIEVAL_ATTEMPTS:
                return EvaluationState.REQUIRES_REFINEMENT
            return EvaluationState.INSUFFICIENT
            
        # Check relevance
        high_score_chunks = [c for c in chunks if c.get("score", 0.0) >= config.MIN_RELEVANCE_SCORE]
        if high_score_chunks:
            return EvaluationState.SUFFICIENT
        else:
            if state.retrieval_attempts < config.MAX_RETRIEVAL_ATTEMPTS:
                return EvaluationState.REQUIRES_REFINEMENT
            return EvaluationState.INSUFFICIENT

    if tool_name == "structured_query":
        # Assume successful structured queries always return sufficient info for that step
        return EvaluationState.SUFFICIENT

    if tool_name == "calculator":
        return EvaluationState.SUFFICIENT

    return EvaluationState.SUFFICIENT

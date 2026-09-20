from typing import Optional
import uuid
import time
from app.schemas.models import (
    AgentRequest, AgentResult, FailureResponse, ErrorCode, ClarificationResponse, AgentState, Intent, Phase6Data
)
from app.agent.normalizer import normalize_query, NormalizerError
from app.agent.intent import detect_intent
from app.agent.planner import Planner
from app.agent.validator import validate_plan, PlanValidationError
from app.agent.executor import execute_plan, AgentExecutionError
from app.agent.evaluator import EvaluationState
from app.context.builder import build_context
from app.clients.phase6_client import call_phase6, Phase6HandoffError
from app.tools.registry import ToolRegistry
from app.logger import get_logger

logger = get_logger("agent")

class Agent:
    def __init__(self, registry: ToolRegistry):
        self.registry = registry
        self.planner = Planner()
        
    def run(self, request: AgentRequest) -> AgentResult:
        query_id = request.query_id or str(uuid.uuid4())
        start_time = time.time()
        
        # Tracking for summary log
        intent_used = None
        tool_count = 0
        outcome = "success"
        
        def finalize(result: AgentResult):
            duration_ms = int((time.time() - start_time) * 1000)
            
            # Map outcome based on result type
            final_outcome = outcome
            if isinstance(result, FailureResponse):
                final_outcome = result.error_code.value if hasattr(result.error_code, "value") else str(result.error_code)
            elif isinstance(result, ClarificationResponse):
                final_outcome = "CLARIFICATION_REQUIRED"
                
            summary = {
                "event": "query_summary",
                "query_id": query_id,
                "intent": intent_used.value if intent_used else "UNKNOWN",
                "duration_ms": duration_ms,
                "tool_count": tool_count,
                "outcome": final_outcome
            }
            logger.info("Query completed", extra={"custom_fields": summary})
            return result
        
        try:
            # 1. Normalize
            try:
                normalized = normalize_query(request.query, max_length=500)
            except NormalizerError as e:
                return finalize(FailureResponse(error_code=e.code, message=e.message, query_id=query_id))
                
            # 2. Detect Intent
            intent_used = detect_intent(normalized)
            if intent_used == Intent.CLARIFICATION_REQUIRED:
                return finalize(ClarificationResponse(clarification_prompt="Could you please clarify your request?"))
                
            # 3. Initialize State
            state = AgentState(
                query_id=query_id,
                identity=request.identity,
                original_query=normalized,
                intent=intent_used,
                conversation_history=request.conversation_history or []
            )
            
            # 4. Plan
            plan = self.planner.create_plan(intent_used, normalized)
            
            # 5. Validate Plan
            try:
                validate_plan(plan, self.registry)
            except PlanValidationError as e:
                return finalize(FailureResponse(error_code=ErrorCode.INTERNAL_ERROR, message=str(e), query_id=query_id))
                
            # 6. Execute Plan
            try:
                state = execute_plan(state, plan, self.registry)
                tool_count = state.tool_calls
            except AgentExecutionError as e:
                tool_count = state.tool_calls
                return finalize(FailureResponse(error_code=e.code, message=e.message, query_id=query_id))
                
            # 7. Check final evaluation
            if state.evaluation_state == EvaluationState.INSUFFICIENT:
                return finalize(FailureResponse(error_code=ErrorCode.INSUFFICIENT_CONTEXT, message="Insufficient context to answer the query.", query_id=query_id))
            elif state.evaluation_state == EvaluationState.REQUIRES_CLARIFICATION:
                return finalize(ClarificationResponse(clarification_prompt="Could you please provide more details?"))
            elif state.evaluation_state == EvaluationState.ERROR:
                return finalize(FailureResponse(error_code=ErrorCode.TOOL_EXECUTION_FAILED, message="Tool execution failed.", query_id=query_id))
                
            # 8. Build Context
            context = build_context(state)
            
            # 9. Handoff Gate
            has_evidence = len(context.retrieved_chunks) > 0
            has_tool_results = context.tool_results is not None and len(context.tool_results) > 0
            if not has_evidence and not has_tool_results:
                return finalize(FailureResponse(error_code=ErrorCode.INSUFFICIENT_CONTEXT, message="Empty evidence set.", query_id=query_id))
                
            # 10. Phase 6 Handoff
            phase6_data = call_phase6(context)
            
            # We return a dict since AgentResult Union says Phase 6 answer data is part of the union.
            # But the signature says AgentResult. If it's Phase6Data model, let's wrap it or return it directly.
            # According to `models.py`, `AgentResult = Union[Phase6Data, FailureResponse, ClarificationResponse]`
            return finalize(phase6_data)
            
        except Phase6HandoffError as e:
            return finalize(FailureResponse(error_code=e.code, message=e.message, query_id=query_id))
        except Exception as e:
            return finalize(FailureResponse(error_code=ErrorCode.INTERNAL_ERROR, message="Unexpected agent error.", query_id=query_id))

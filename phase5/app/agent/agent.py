from typing import Optional
import uuid
from app.schemas.models import (
    AgentRequest, AgentResult, FailureResponse, ErrorCode, ClarificationResponse, AgentState, Intent
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

class Agent:
    def __init__(self, registry: ToolRegistry):
        self.registry = registry
        self.planner = Planner()
        
    def run(self, request: AgentRequest) -> AgentResult:
        query_id = request.query_id or str(uuid.uuid4())
        
        try:
            # 1. Normalize
            try:
                normalized = normalize_query(request.query, max_length=500)
            except NormalizerError as e:
                return FailureResponse(error_code=e.code, message=e.message, query_id=query_id)
                
            # 2. Detect Intent
            intent = detect_intent(normalized)
            if intent == Intent.CLARIFICATION_REQUIRED:
                return ClarificationResponse(clarification_prompt="Could you please clarify your request?")
                
            # 3. Initialize State
            state = AgentState(
                query_id=query_id,
                identity=request.identity,
                original_query=normalized,
                intent=intent,
                conversation_history=request.conversation_history or []
            )
            
            # 4. Plan
            plan = self.planner.create_plan(intent, normalized)
            
            # 5. Validate Plan
            try:
                validate_plan(plan, self.registry)
            except PlanValidationError as e:
                return FailureResponse(error_code=ErrorCode.INTERNAL_ERROR, message=str(e), query_id=query_id)
                
            # 6. Execute Plan
            try:
                state = execute_plan(state, plan, self.registry)
            except AgentExecutionError as e:
                # E.g. limit exceeded, tool failed
                return FailureResponse(error_code=e.code, message=e.message, query_id=query_id)
                
            # 7. Check final evaluation
            if state.evaluation_state == EvaluationState.INSUFFICIENT:
                return FailureResponse(error_code=ErrorCode.INSUFFICIENT_CONTEXT, message="Insufficient context to answer the query.", query_id=query_id)
            elif state.evaluation_state == EvaluationState.REQUIRES_CLARIFICATION:
                return ClarificationResponse(clarification_prompt="Could you please provide more details?")
            elif state.evaluation_state == EvaluationState.ERROR:
                return FailureResponse(error_code=ErrorCode.TOOL_EXECUTION_FAILED, message="Tool execution failed.", query_id=query_id)
                
            # 8. Build Context
            context = build_context(state)
            
            # 9. Handoff Gate
            has_evidence = len(context.retrieved_chunks) > 0
            has_tool_results = context.tool_results is not None and len(context.tool_results) > 0
            if not has_evidence and not has_tool_results:
                return FailureResponse(error_code=ErrorCode.INSUFFICIENT_CONTEXT, message="Empty evidence set.", query_id=query_id)
                
            # 10. Phase 6 Handoff
            phase6_data = call_phase6(context)
            return phase6_data
            
        except Phase6HandoffError as e:
            return FailureResponse(error_code=e.code, message=e.message, query_id=query_id)
        except Exception as e:
            return FailureResponse(error_code=ErrorCode.INTERNAL_ERROR, message="Unexpected agent error.", query_id=query_id)

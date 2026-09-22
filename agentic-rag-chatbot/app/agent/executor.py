from typing import List, Dict, Any
from app.schemas.models import (
    AgentState, PlanStep, Step, EvaluationState, Chunk, ErrorCode
)
from app.tools.registry import ToolRegistry
from app.agent.evaluator import evaluate_result
from app.agent.refiner import refine_query
from app.config import get_config

class AgentExecutionError(Exception):
    def __init__(self, code: ErrorCode, message: str):
        self.code = code
        self.message = message
        super().__init__(self.message)

def execute_plan(state: AgentState, plan: List[PlanStep], registry: ToolRegistry) -> AgentState:
    config = get_config()
    
    # We use a mutable copy of the plan so we can prepend steps (e.g. for refinement)
    remaining_plan = list(plan)
    
    while remaining_plan:
        if state.tool_calls >= config.MAX_TOOL_CALLS:
            state.evaluation_state = EvaluationState.ERROR
            raise AgentExecutionError(ErrorCode.AGENT_EXECUTION_LIMIT, "Maximum tool calls exceeded.")
            
        if state.plan_steps >= config.MAX_PLAN_STEPS:
            state.evaluation_state = EvaluationState.ERROR
            raise AgentExecutionError(ErrorCode.AGENT_EXECUTION_LIMIT, "Maximum plan steps exceeded.")
            
        current_step = remaining_plan.pop(0)
        state.plan_steps += 1
        
        import re
        
        def resolve_inputs(tool_input: dict, state: AgentState) -> dict:
            resolved = {}
            for k, v in tool_input.items():
                if isinstance(v, str):
                    def replace_match(match):
                        step_idx = int(match.group(1))
                        if step_idx < len(state.tool_results_list):
                            res = state.tool_results_list[step_idx]["output"]
                            if isinstance(res, dict) and "value" in res:
                                return str(res["value"])
                            elif isinstance(res, dict) and "results" in res and len(res["results"]) > 0:
                                # For structured query mock
                                return str(res["results"][0].get("value", res["results"][0]))
                            return str(res)
                        return match.group(0)
                    resolved_v = re.sub(r"\{step_(\d+)\}", replace_match, v)
                    resolved[k] = resolved_v
                else:
                    resolved[k] = v
            return resolved

        # Identity enforcement (from state)
        tool_input = dict(current_step.tool_input)
        
        # Execute tool
        state.tool_calls += 1
        
        if current_step.tool == "vector_search":
            state.retrieval_attempts += 1
            # Ensure query_text is injected from current state if not explicitly hardcoded in plan
            if "query_text" not in tool_input or not tool_input["query_text"]:
                tool_input["query_text"] = state.refined_query or state.original_query
        
        resolved_input = resolve_inputs(tool_input, state)
        
        result = registry.execute_tool(current_step.tool, resolved_input, state.identity)
        
        # Evaluate
        eval_state = evaluate_result(current_step.tool, result, state)
        state.evaluation_state = eval_state
        
        # Record context
        step_record = Step(
            step=current_step.description,
            tool=current_step.tool,
            result_summary="Success" if result.success else f"Error: {result.error}"
        )
        state.steps_taken.append(step_record)
        
        # Handle specific tool data
        if result.success:
            if current_step.tool == "vector_search" and result.data and "chunks" in result.data:
                # Add chunks to state (deduping handled in context builder later)
                for chunk_dict in result.data["chunks"]:
                    state.retrieved_chunks.append(Chunk(**chunk_dict))
            elif current_step.tool in ["structured_query", "calculator"]:
                # Store in tool_results_list if we want it in context
                state.tool_results_list.append({
                    "tool": current_step.tool,
                    "input": tool_input,
                    "output": result.data
                })

        # Handle evaluation outcome
        if eval_state == EvaluationState.SUFFICIENT:
            continue
        elif eval_state == EvaluationState.REQUIRES_REFINEMENT:
            if state.retrieval_attempts >= config.MAX_RETRIEVAL_ATTEMPTS:
                state.evaluation_state = EvaluationState.INSUFFICIENT
                break
            # Refine and try again
            state.refined_query = refine_query(state)
            
            new_input = {"query_text": state.refined_query}
            if "document_ids" in current_step.tool_input:
                new_input["document_ids"] = current_step.tool_input["document_ids"]
            if "filters" in current_step.tool_input:
                new_input["filters"] = current_step.tool_input["filters"]
                
            # Re-inject vector_search step
            retry_step = PlanStep(
                description="Refined retrieval",
                tool="vector_search",
                tool_input=new_input
            )
            remaining_plan.insert(0, retry_step)
        elif eval_state == EvaluationState.REQUIRES_ANOTHER_TOOL:
            # We don't have a complex rule for this yet, treat as insufficient
            state.evaluation_state = EvaluationState.INSUFFICIENT
            break
        elif eval_state == EvaluationState.REQUIRES_CLARIFICATION:
            break
        elif eval_state == EvaluationState.INSUFFICIENT:
            break
        elif eval_state == EvaluationState.ERROR:
            raise AgentExecutionError(ErrorCode.TOOL_EXECUTION_FAILED, f"Tool {current_step.tool} failed.")
            
    return state

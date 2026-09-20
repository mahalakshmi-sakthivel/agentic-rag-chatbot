from typing import List
from app.schemas.models import PlanStep
from app.tools.registry import ToolRegistry

class PlanValidationError(Exception):
    pass

def validate_plan(plan: List[PlanStep], registry: ToolRegistry, max_steps: int = 8) -> bool:
    if len(plan) > max_steps:
        raise PlanValidationError(f"Plan exceeds maximum allowed steps ({max_steps})")
        
    for step in plan:
        tool = registry.get_tool(step.tool)
        if not tool:
            raise PlanValidationError(f"Unregistered tool requested: {step.tool}")
            
        if not isinstance(step.tool_input, dict):
            raise PlanValidationError(f"Tool input must be a dictionary for tool: {step.tool}")
            
        # no identity fields in step input
        if "user_id" in step.tool_input or "tenant_id" in step.tool_input:
            raise PlanValidationError("Identity fields (user_id, tenant_id) are not allowed in tool input")
            
    return True

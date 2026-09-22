from typing import List, Protocol, Optional
from app.schemas.models import Intent, PlanStep

class LLMPlannerHook(Protocol):
    def generate_plan(self, intent: Intent, query: str) -> List[PlanStep]:
        ...

class Planner:
    def __init__(self, llm_hook: Optional[LLMPlannerHook] = None):
        self.llm_hook = llm_hook

    def create_plan(self, intent: Intent, query: str) -> List[PlanStep]:
        if self.llm_hook is not None:
            return self.llm_hook.generate_plan(intent, query)
            
        # Deterministic rule-based templates
        if intent == Intent.DOCUMENT_LOOKUP:
            return [
                PlanStep(description="Retrieve relevant documents", tool="vector_search", tool_input={"query_text": query})
            ]
        elif intent == Intent.STRUCTURED_DATA_QUERY:
            return [
                PlanStep(description="Retrieve structured data", tool="structured_query", tool_input={"query": query})
            ]
        elif intent == Intent.CALCULATION:
            return [
                PlanStep(description="Calculate result", tool="calculator", tool_input={"expression": query})
            ]
        elif intent == Intent.COMPARISON:
            return [
                PlanStep(description="Retrieve first item data", tool="vector_search", tool_input={"query_text": query}),
                PlanStep(description="Retrieve second item data", tool="vector_search", tool_input={"query_text": query})
            ]
        elif intent == Intent.MULTI_STEP:
            # Deterministic multi-step is unsafe without LLM planning.
            # We require LLM planning to construct actual calculation expressions
            # and pass validated outputs between steps. We should not fabricate it.
            return []
        elif intent == Intent.CLARIFICATION_REQUIRED:
            return []
            
        return [
            PlanStep(description="Default retrieval", tool="vector_search", tool_input={"query_text": query})
        ]

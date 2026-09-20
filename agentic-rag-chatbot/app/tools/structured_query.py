from typing import Protocol, Dict, Any
from app.tools.base import BaseTool
from app.schemas.models import Identity, ToolResult, ErrorCode

class StructuredQueryBackend(Protocol):
    def execute_query(self, query: str, identity: Identity) -> Dict[str, Any]:
        ...

class MockStructuredQueryBackend:
    def execute_query(self, query: str, identity: Identity) -> Dict[str, Any]:
        return {"result": f"Mocked structured data for '{query}'"}

class StructuredQueryTool(BaseTool):
    name = "structured_query"

    def __init__(self, backend: StructuredQueryBackend):
        self.backend = backend

    def execute(self, input_data: Any, identity_context: Identity) -> ToolResult:
        if not isinstance(input_data, dict):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)
        
        query = input_data.get("query")
        if not query or not isinstance(query, str):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)

        try:
            result = self.backend.execute_query(query, identity_context)
            return ToolResult(success=True, data=result)
        except Exception:
            return ToolResult(success=False, error=ErrorCode.TOOL_EXECUTION_FAILED.value)

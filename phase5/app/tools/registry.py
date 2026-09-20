from typing import Dict, Optional
from app.tools.base import BaseTool
from app.schemas.models import ErrorCode

class ToolRegistry:
    def __init__(self):
        self._tools: Dict[str, BaseTool] = {}

    def register(self, tool: BaseTool):
        self._tools[tool.name] = tool

    def get_tool(self, name: str) -> Optional[BaseTool]:
        return self._tools.get(name)

    def execute_tool(self, name: str, input_data: dict, identity_context: 'Identity') -> 'ToolResult':
        tool = self.get_tool(name)
        if not tool:
            from app.schemas.models import ToolResult
            return ToolResult(
                success=False,
                error=ErrorCode.TOOL_NOT_FOUND.value
            )
        try:
            return tool.execute(input_data, identity_context)
        except Exception as e:
            from app.schemas.models import ToolResult
            return ToolResult(
                success=False,
                error=ErrorCode.TOOL_EXECUTION_FAILED.value
            )

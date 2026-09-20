from typing import Any
from abc import ABC, abstractmethod
from app.schemas.models import Identity, ToolResult

class BaseTool(ABC):
    name: str

    @abstractmethod
    def execute(self, input_data: Any, identity_context: Identity) -> ToolResult:
        pass

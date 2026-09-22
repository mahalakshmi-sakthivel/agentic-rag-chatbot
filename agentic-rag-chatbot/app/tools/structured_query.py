from typing import Protocol, Dict, Any
from app.tools.base import BaseTool
from app.schemas.models import Identity, ToolResult, ErrorCode

class StructuredQueryBackend(Protocol):
    def execute_query(self, query: str, identity: Identity) -> Dict[str, Any]:
        ...

class MockStructuredQueryBackend:
    def execute_query(self, query: str, identity: Identity) -> Dict[str, Any]:
        return {"result": f"Mocked structured data for '{query}'"}

import httpx
import re
from app.config import get_config

class RealStructuredQueryBackend:
    def execute_query(self, query: str, identity: Identity) -> Dict[str, Any]:
        # Basic SQL injection / unsafe query validation
        unsafe_patterns = [r"(?i)\bDROP\b", r"(?i)\bDELETE\b", r"(?i)\bUPDATE\b", r"(?i)\bINSERT\b", r"(?i)\bALTER\b"]
        for p in unsafe_patterns:
            if re.search(p, query):
                raise ValueError("Unsafe query detected")
                
        config = get_config()
        # Ensure client is created properly if using httpx
        url = f"{config.PHASE4_BASE_URL.rstrip('/')}/v1/retrieval/structured"
        
        headers = {
            "Content-Type": "application/json",
            "X-Internal-Auth": config.INTERNAL_SERVICE_TOKEN
        }
        
        payload = {
            "query_text": query,
            "tenant_id": identity.tenant_id,
            "user_id": identity.user_id
        }
        
        timeout = httpx.Timeout(config.PHASE4_TIMEOUT_MS / 1000.0)
        
        try:
            with httpx.Client() as client:
                response = client.post(url, json=payload, headers=headers, timeout=timeout)
                if response.status_code == 200:
                    return response.json()
                elif response.status_code in (401, 403):
                    raise ValueError("Permission denied by backend")
                else:
                    raise Exception(f"Backend error {response.status_code}")
        except httpx.RequestError as e:
            raise Exception(f"Network error: {e}")

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

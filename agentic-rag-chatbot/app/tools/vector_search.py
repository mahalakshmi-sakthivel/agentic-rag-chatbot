from typing import Any, Dict
from app.tools.base import BaseTool
from app.schemas.models import Identity, ToolResult, Chunk, ErrorCode
from app.clients.phase4_client import Phase4Client, Phase4Request
from app.config import get_config

class VectorSearchTool(BaseTool):
    name = "vector_search"

    def __init__(self, client: Phase4Client):
        self.client = client
        self.config = get_config()

    def execute(self, input_data: Any, identity_context: Identity) -> ToolResult:
        if not isinstance(input_data, dict):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)
            
        query_text = input_data.get("query_text")
        if not query_text or not isinstance(query_text, str):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)

        # Force tenant_id and user_id from identity
        # Reject invalid top_k
        requested_top_k = input_data.get("top_k", 5)
        if not isinstance(requested_top_k, int):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)
        
        if requested_top_k > self.config.MAX_TOP_K or requested_top_k < 1:
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)
            
        top_k = requested_top_k
        
        document_ids = input_data.get("document_ids")
        if document_ids is not None and not isinstance(document_ids, list):
            document_ids = None

        filters = input_data.get("filters")
        if filters is not None and not isinstance(filters, dict):
            filters = None

        req = Phase4Request(
            query_text=query_text,
            tenant_id=identity_context.tenant_id,
            user_id=identity_context.user_id,
            document_ids=document_ids,
            top_k=top_k,
            filters=filters
        )

        try:
            resp = self.client.search(req)
            chunks = []
            for r in resp.results:
                chunks.append(Chunk(
                    chunk_id=r.chunk_id,
                    document_id=r.document_id,
                    text=r.text,
                    score=r.score,
                    source_location=r.source_location,
                    filename=r.filename
                ))
            return ToolResult(success=True, data={"chunks": [c.model_dump() for c in chunks]})
        except Exception:
            return ToolResult(success=False, error=ErrorCode.TOOL_EXECUTION_FAILED.value)

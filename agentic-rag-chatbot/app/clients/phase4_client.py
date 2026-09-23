from typing import Protocol, List, Dict, Any, Optional
from pydantic import BaseModel

class Phase4Request(BaseModel):
    query_text: str
    tenant_id: str
    user_id: str
    roles: List[str]
    session_id: str
    document_ids: Optional[List[str]] = None
    top_k: int = 5
    filters: Optional[Dict[str, Any]] = None

class Phase4Result(BaseModel):
    chunk_id: str
    document_id: str
    text: str
    score: float
    source_location: str
    filename: str

class Phase4Response(BaseModel):
    results: List[Phase4Result]

class Phase4Client(Protocol):
    def search(self, request: Phase4Request) -> Phase4Response:
        ...

import httpx
from app.config import get_config
from app.schemas.models import ErrorCode
from typing import Optional

class Phase4HandoffError(Exception):
    def __init__(self, code: ErrorCode, message: str):
        self.code = code
        self.message = message
        super().__init__(self.message)

_client: Optional[httpx.Client] = None

def get_httpx_client() -> httpx.Client:
    global _client
    if _client is None:
        _client = httpx.Client()
    return _client

class RealPhase4Client:
    def search(self, request: Phase4Request) -> Phase4Response:
        config = get_config()
        client = get_httpx_client()
        url = f"{config.PHASE4_BASE_URL.rstrip('/')}/v1/retrieval/search"
        
        headers = {
            "Content-Type": "application/json",
            "X-Internal-Auth": config.INTERNAL_SERVICE_TOKEN
        }
        
        payload = request.model_dump(exclude_none=True)
        timeout = httpx.Timeout(config.PHASE4_TIMEOUT_MS / 1000.0)
        
        try:
            response = client.post(url, json=payload, headers=headers, timeout=timeout)
            
            if response.status_code == 200:
                return Phase4Response(**response.json())
            
            # Map errors
            if response.status_code == 400:
                raise Phase4HandoffError(ErrorCode.VALIDATION_ERROR, "Phase 4 validation error")
            elif response.status_code == 401 or response.status_code == 403:
                raise Phase4HandoffError(ErrorCode.UNAUTHENTICATED, "Phase 4 permission denied")
            elif response.status_code == 504:
                raise Phase4HandoffError(ErrorCode.RETRIEVAL_TIMEOUT, "Phase 4 timeout")
            else:
                raise Phase4HandoffError(ErrorCode.INTERNAL_ERROR, f"Phase 4 error: {response.status_code}")
                
        except (httpx.TimeoutException, httpx.ReadTimeout):
            raise Phase4HandoffError(ErrorCode.RETRIEVAL_TIMEOUT, "Phase 4 client timeout")
        except httpx.RequestError:
            raise Phase4HandoffError(ErrorCode.INTERNAL_ERROR, "Phase 4 network error")

class MockPhase4Client:
    def search(self, request: Phase4Request) -> Phase4Response:
        # A simple mock that returns a dummy result if query is not empty
        if not request.query_text.strip():
            return Phase4Response(results=[])
            
        return Phase4Response(results=[
            Phase4Result(
                chunk_id="mock_chunk_1",
                document_id="mock_doc_1",
                text=f"Mock result for {request.query_text}",
                score=0.95,
                source_location="page 1",
                filename="mock_file.pdf"
            )
        ])

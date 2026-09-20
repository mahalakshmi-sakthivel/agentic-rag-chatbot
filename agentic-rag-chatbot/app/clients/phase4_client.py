from typing import Protocol, List, Dict, Any, Optional
from pydantic import BaseModel

class Phase4Request(BaseModel):
    query_text: str
    tenant_id: str
    user_id: str
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

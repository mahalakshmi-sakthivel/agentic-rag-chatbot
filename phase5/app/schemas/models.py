from pydantic import BaseModel, ConfigDict, Field
from typing import List, Optional, Any, Dict
from enum import Enum

# Enums
class Intent(str, Enum):
    DOCUMENT_LOOKUP = "DOCUMENT_LOOKUP"
    STRUCTURED_DATA_QUERY = "STRUCTURED_DATA_QUERY"
    CALCULATION = "CALCULATION"
    COMPARISON = "COMPARISON"
    MULTI_STEP = "MULTI_STEP"
    CLARIFICATION_REQUIRED = "CLARIFICATION_REQUIRED"
    NO_RETRIEVAL_REQUIRED = "NO_RETRIEVAL_REQUIRED"

class EvaluationState(str, Enum):
    SUFFICIENT = "SUFFICIENT"
    INSUFFICIENT = "INSUFFICIENT"
    REQUIRES_REFINEMENT = "REQUIRES_REFINEMENT"
    REQUIRES_ANOTHER_TOOL = "REQUIRES_ANOTHER_TOOL"
    REQUIRES_CLARIFICATION = "REQUIRES_CLARIFICATION"
    ERROR = "ERROR"

class ErrorCode(str, Enum):
    EMPTY_QUERY = "EMPTY_QUERY"
    NO_DOCUMENTS_INDEXED = "NO_DOCUMENTS_INDEXED"
    RETRIEVAL_TIMEOUT = "RETRIEVAL_TIMEOUT"
    TOOL_NOT_FOUND = "TOOL_NOT_FOUND"
    TOOL_INPUT_INVALID = "TOOL_INPUT_INVALID"
    TOOL_EXECUTION_FAILED = "TOOL_EXECUTION_FAILED"
    AGENT_EXECUTION_LIMIT = "AGENT_EXECUTION_LIMIT"
    INSUFFICIENT_CONTEXT = "INSUFFICIENT_CONTEXT"
    CLARIFICATION_REQUIRED = "CLARIFICATION_REQUIRED"
    VALIDATION_ERROR = "VALIDATION_ERROR"
    UNAUTHENTICATED = "UNAUTHENTICATED"
    LLM_RATE_LIMIT = "LLM_RATE_LIMIT"
    LLM_PROVIDER_ERROR = "LLM_PROVIDER_ERROR"
    LLM_TIMEOUT = "LLM_TIMEOUT"
    LLM_CONTENT_FILTERED = "LLM_CONTENT_FILTERED"
    INTERNAL_ERROR = "INTERNAL_ERROR"

# Models
class Identity(BaseModel):
    model_config = ConfigDict(frozen=True)
    user_id: str
    tenant_id: str

class Chunk(BaseModel):
    chunk_id: str
    document_id: str
    text: str
    score: float
    source_location: str
    filename: str

class Step(BaseModel):
    step: str
    tool: str
    result_summary: str

class PlanStep(BaseModel):
    description: str
    tool: str
    tool_input: Dict[str, Any]

class OrchestratorContext(BaseModel):
    model_config = ConfigDict(extra='forbid')
    query_id: str
    identity: Identity
    original_query: str
    retrieved_chunks: List[Chunk]
    refined_query: Optional[str] = None
    steps_taken: Optional[List[Step]] = None
    conversation_history: Optional[List[Dict[str, Any]]] = None
    # ASSUMPTION: added tool_results to support non-retrieval data as per spec open items
    tool_results: Optional[List[Dict[str, Any]]] = None

class Phase6Validation(BaseModel):
    groundedness: str
    unsupported_claims: List[str]
    citation_coverage: float
    passed: bool

class Phase6Source(BaseModel):
    document_id: str
    chunk_id: str
    excerpt: str
    score: float
    page_or_location: str

class Phase6Data(BaseModel):
    query_id: str
    answer: str
    sources: List[Phase6Source]
    validation: Phase6Validation
    llm_ms: int

class Phase6Meta(BaseModel):
    request_id: str
    timestamp: str

class Phase6SuccessResponse(BaseModel):
    success: bool
    data: Phase6Data
    meta: Phase6Meta

class Phase6ErrorDetail(BaseModel):
    code: str # Can be one of ErrorCode, but we use str just in case
    message: str
    details: Dict[str, Any]

class Phase6ErrorResponse(BaseModel):
    success: bool
    error: Phase6ErrorDetail
    meta: Phase6Meta

class ToolResult(BaseModel):
    success: bool
    data: Optional[Any] = None
    error: Optional[str] = None

class AgentState(BaseModel):
    query_id: str
    identity: Identity
    original_query: str
    refined_query: Optional[str] = None
    intent: Optional[Intent] = None
    retrieval_attempts: int = 0
    tool_calls: int = 0
    plan_steps: int = 0
    clarification_attempts: int = 0
    retrieved_chunks: List[Chunk] = Field(default_factory=list)
    steps_taken: List[Step] = Field(default_factory=list)
    tool_results_list: List[Dict[str, Any]] = Field(default_factory=list)
    evaluation_state: Optional[EvaluationState] = None
    conversation_history: List[Dict[str, Any]] = Field(default_factory=list)
    
class AgentRequest(BaseModel):
    query: str
    identity: Identity
    query_id: Optional[str] = None
    document_ids: Optional[List[str]] = None
    conversation_history: Optional[List[Dict[str, Any]]] = None
    top_k: Optional[int] = None

class ClarificationResponse(BaseModel):
    needs_clarification: bool = True
    clarification_prompt: str

class FailureResponse(BaseModel):
    # ASSUMPTION: Contract-compatible failure shape is standard success/error_code/message with query_id
    success: bool = False
    error_code: ErrorCode
    message: str
    query_id: str
AgentResult = Phase6Data | ClarificationResponse | FailureResponse

import httpx
from app.schemas.models import OrchestratorContext, Phase6Data, Phase6SuccessResponse, Phase6ErrorResponse, ErrorCode
from app.config import get_config

class Phase6HandoffError(Exception):
    def __init__(self, code: ErrorCode, message: str):
        self.code = code
        self.message = message
        super().__init__(self.message)

# Shared HTTP client for connection pooling
_client = None

def get_httpx_client() -> httpx.Client:
    global _client
    if _client is None:
        _client = httpx.Client()
    return _client

def reset_httpx_client():
    global _client
    if _client is not None:
        _client.close()
        _client = None

def call_phase6(context: OrchestratorContext) -> Phase6Data:
    config = get_config()
    client = get_httpx_client()
    url = f"{config.PHASE6_BASE_URL.rstrip('/')}/internal/v1/llm/generate"
    
    headers = {
        "Content-Type": "application/json",
        "X-Request-ID": context.query_id,
        "X-Internal-Auth": config.INTERNAL_SERVICE_TOKEN
    }
    
    payload = context.model_dump(exclude_none=True)
    timeout = httpx.Timeout(config.PHASE6_TIMEOUT_MS / 1000.0)
    
    attempts = 0
    max_attempts = config.PHASE6_MAX_RETRIES + 1
    
    while attempts < max_attempts:
        attempts += 1
        try:
            response = client.post(url, json=payload, headers=headers, timeout=timeout)
            
            if response.status_code == 200:
                resp_data = Phase6SuccessResponse(**response.json())
                return resp_data.data
                
            # Parse error envelope
            try:
                error_resp = Phase6ErrorResponse(**response.json())
                err_code = error_resp.error.code
            except Exception:
                # If we can't parse the error envelope, map based on status
                if response.status_code in (400, 401, 500):
                    raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Phase 6 internal or integration error")
                elif response.status_code == 429:
                    raise Phase6HandoffError(ErrorCode.LLM_RATE_LIMIT, "Phase 6 rate limited")
                elif response.status_code == 502:
                    raise Phase6HandoffError(ErrorCode.LLM_PROVIDER_ERROR, "Phase 6 provider error")
                elif response.status_code == 504:
                    raise Phase6HandoffError(ErrorCode.LLM_TIMEOUT, "Phase 6 timeout")
                else:
                    raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Phase 6 unhandled error")
            
            # Map explicit error codes
            if response.status_code == 400: # VALIDATION_ERROR
                raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Integration defect: context schema invalid")
            elif response.status_code == 401: # UNAUTHENTICATED
                raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Integration defect: unauthenticated")
            elif response.status_code == 500: # INTERNAL_ERROR
                raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Phase 6 internal error")
            elif response.status_code == 429: # LLM_RATE_LIMIT
                raise Phase6HandoffError(ErrorCode.LLM_RATE_LIMIT, "Rate limited")
            elif response.status_code == 502: # LLM_PROVIDER_ERROR
                raise Phase6HandoffError(ErrorCode.LLM_PROVIDER_ERROR, "Provider failed")
            elif response.status_code == 504: # LLM_TIMEOUT
                raise Phase6HandoffError(ErrorCode.LLM_TIMEOUT, "LLM timeout")
            else:
                # E.g. LLM_CONTENT_FILTERED
                # We return it upward directly, mapping the string code to ErrorCode if possible
                try:
                    mapped_code = ErrorCode(err_code)
                except ValueError:
                    mapped_code = ErrorCode.INTERNAL_ERROR
                raise Phase6HandoffError(mapped_code, "Phase 6 error")
                
        except httpx.ConnectError as e:
            if attempts >= max_attempts:
                raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Phase 6 unreachable")
            continue # Retry
        except (httpx.TimeoutException, httpx.ReadTimeout) as e:
            # No retry on timeout
            raise Phase6HandoffError(ErrorCode.LLM_TIMEOUT, "Phase 6 client timeout")
        except httpx.RequestError as e:
            # Other network errors without retry
            raise Phase6HandoffError(ErrorCode.INTERNAL_ERROR, "Phase 6 network error")

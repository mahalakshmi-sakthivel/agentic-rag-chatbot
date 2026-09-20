import re
from app.schemas.models import ErrorCode

class NormalizerError(Exception):
    def __init__(self, code: ErrorCode, message: str):
        self.code = code
        self.message = message
        super().__init__(self.message)

def normalize_query(query: str, max_length: int = 500) -> str:
    if not query:
        raise NormalizerError(ErrorCode.EMPTY_QUERY, "Query is empty.")
    
    # Strip and collapse whitespace
    normalized = re.sub(r'\s+', ' ', query.strip())
    
    if not normalized:
        raise NormalizerError(ErrorCode.EMPTY_QUERY, "Query is empty after normalization.")
        
    # Cap length
    if len(normalized) > max_length:
        normalized = normalized[:max_length].strip()
        
    return normalized

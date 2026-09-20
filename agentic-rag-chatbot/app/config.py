import os
from typing import Optional

def get_env_or_fail(key: str) -> str:
    val = os.getenv(key)
    if val is None:
        raise ValueError(f"Missing required environment variable: {key}")
    return val

def get_env_int(key: str, default: int) -> int:
    val = os.getenv(key)
    if val is None:
        return default
    return int(val)

def get_env_float(key: str, default: float) -> float:
    val = os.getenv(key)
    if val is None:
        return default
    return float(val)

def get_env_bool(key: str, default: bool) -> bool:
    val = os.getenv(key)
    if val is None:
        return default
    return str(val).lower() in ("1", "true", "yes")

class Config:
    # Phase 6 Handoff
    PHASE6_BASE_URL: str = os.getenv("PHASE6_BASE_URL", "http://localhost:8006") # Not strictly required by default but fail if None? Spec says "fail clearly if a required one is missing".
    INTERNAL_SERVICE_TOKEN: str = os.getenv("INTERNAL_SERVICE_TOKEN", "")
    
    def __init__(self):
        # We will initialize in a method to allow lazy loading or test overriding, but let's just do it directly.
        self.PHASE6_BASE_URL = get_env_or_fail("PHASE6_BASE_URL")
        self.INTERNAL_SERVICE_TOKEN = get_env_or_fail("INTERNAL_SERVICE_TOKEN")
        self.PHASE6_TIMEOUT_MS = get_env_int("PHASE6_TIMEOUT_MS", 10000)
        self.PHASE6_MAX_RETRIES = get_env_int("PHASE6_MAX_RETRIES", 1)
        
        # ASSUMPTION: MIN_RELEVANCE_SCORE is a float
        self.MIN_RELEVANCE_SCORE = get_env_float("MIN_RELEVANCE_SCORE", 0.5)
        self.INCLUDE_TOOL_RESULTS = get_env_bool("INCLUDE_TOOL_RESULTS", False)

        # Limits (from spec 18)
        self.MAX_RETRIEVAL_ATTEMPTS = get_env_int("MAX_RETRIEVAL_ATTEMPTS", 2)
        self.MAX_TOOL_CALLS = get_env_int("MAX_TOOL_CALLS", 5)
        self.MAX_PLAN_STEPS = get_env_int("MAX_PLAN_STEPS", 8)
        self.MAX_CLARIFICATION_ATTEMPTS = get_env_int("MAX_CLARIFICATION_ATTEMPTS", 1)
        self.MAX_TOP_K = get_env_int("MAX_TOP_K", 20)
        self.MAX_PHASE6_CALLS = get_env_int("MAX_PHASE6_CALLS", 1)

try:
    # Creating a global config instance. 
    # If variables are missing, we might want to delay loading until needed, or fail at import. 
    # Let's use a function to load config so tests can mock env vars easily.
    pass
except Exception:
    pass

_config = None

def get_config() -> Config:
    global _config
    if _config is None:
        _config = Config()
    return _config

def clear_config():
    global _config
    _config = None

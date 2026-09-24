# app/config.py
#
# Phase 6 configuration. Mirrors Phase 4's dataclass-settings pattern so the
# repo stays consistent across the Python phases (§31 open item #2: only
# Phase 1 is C++-only).
#
# INTERNAL_SERVICE_TOKEN is the shared secret Phase 5 sends in the
# `X-Internal-Auth` header (see Phase 5 §19A.6). It must never be committed;
# set it via the environment / docker-compose in real deployments.
import os
from dataclasses import dataclass, field


def _get_int(key: str, default: int) -> int:
    val = os.getenv(key)
    return int(val) if val is not None else default


def _get_float(key: str, default: float) -> float:
    val = os.getenv(key)
    return float(val) if val is not None else default


@dataclass(frozen=True)
class Settings:
    # --- Internal auth (Phase 5 -> Phase 6 handoff, §19A.6) ---
    internal_service_token: str = os.getenv("INTERNAL_SERVICE_TOKEN", "")

    # --- LLM provider ---
    # Default provider is local Ollama running Qwen3 8B (qwen3:8b) over HTTP.
    # LLMClient is provider-agnostic (see app/llm_client.py), supporting
    # "ollama" (default), "openai_compatible", and "mock" (for tests/CI).
    llm_provider: str = os.getenv("LLM_PROVIDER", "ollama")
    llm_api_key: str = os.getenv("LLM_API_KEY", "")
    llm_model_name: str = os.getenv("LLM_MODEL_NAME", "qwen3:8b")
    llm_base_url: str = os.getenv("LLM_BASE_URL", "http://host.docker.internal:11434")
    llm_timeout_ms: int = _get_int("LLM_TIMEOUT_MS", 8000)
    llm_temperature: float = _get_float("LLM_TEMPERATURE", 0.2)
    llm_max_tokens: int = _get_int("LLM_MAX_TOKENS", 500)

    # --- Token / context-window budgeting ---
    # Approximate token counting (chars/4) to enforce prompt budget limits
    # (max_chunks_in_prompt, max_context_tokens).
    max_context_tokens: int = _get_int("MAX_CONTEXT_TOKENS", 6000)
    max_chunks_in_prompt: int = _get_int("MAX_CHUNKS_IN_PROMPT", 20)

    # --- Validation thresholds ---
    min_citation_coverage: float = _get_float("MIN_CITATION_COVERAGE", 0.6)

    # --- Regeneration ---
    allow_regeneration: bool = os.getenv("ALLOW_REGENERATION", "true").lower() in ("1", "true", "yes")


settings = Settings()

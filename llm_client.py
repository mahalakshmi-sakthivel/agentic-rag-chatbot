# app/llm_client.py
#
# LLMClient abstraction (Part 7 of the Phase 6 planning discussion).
# The default provider is local Ollama running Qwen3 8B (qwen3:8b), while
# the LLMClient interface remains provider-agnostic so adapters can be swapped
# via LLM_PROVIDER without touching prompt_builder.py, citation.py,
# validation.py, or services.py.
#
# Supported providers:
# - "ollama" (default): local Ollama HTTP API with Qwen3 8B
# - "openai_compatible": generic HTTP adapter for OpenAI-compatible endpoints
# - "mock": zero-dependency mock for automated tests and CI
from __future__ import annotations

import time
from abc import ABC, abstractmethod
from dataclasses import dataclass

import httpx

from .config import settings
from .errors import LlmContentFilteredError, LlmProviderError, LlmRateLimitError, LlmTimeoutError


@dataclass
class LLMResponse:
    text: str
    llm_ms: int


class LLMClient(ABC):
    """Provider-agnostic interface. Every concrete provider adapter must
    raise Phase6Exception subclasses from app/errors.py on failure — never
    a raw provider-specific exception — so the FastAPI layer can map errors
    consistently regardless of which provider is active."""

    @abstractmethod
    def generate(self, prompt: str) -> LLMResponse:
        ...


class MockLLMClient(LLMClient):
    """Zero-dependency mock implementation for automated testing and CI.
    It does not call an external model. It extracts the first cited chunk
    from the prompt and echoes it back with a citation tag, enabling the
    rest of the pipeline (citation mapping, validation, regeneration)
    to be tested deterministically."""

    def generate(self, prompt: str) -> LLMResponse:
        start = time.monotonic()
        chunk_id = None
        for line in prompt.splitlines():
            if line.strip().startswith("<context chunk_id="):
                start_idx = line.find('chunk_id="') + len('chunk_id="')
                end_idx = line.find('"', start_idx)
                chunk_id = line[start_idx:end_idx]
                break

        if chunk_id:
            text = (
                f"Based on the provided documents, here is the relevant "
                f"information. [chunk:{chunk_id}]"
            )
        else:
            text = "The provided documents do not contain enough information to answer this question."

        elapsed_ms = int((time.monotonic() - start) * 1000)
        return LLMResponse(text=text, llm_ms=elapsed_ms)


class OpenAICompatibleClient(LLMClient):
    """Generic adapter for any OpenAI-Chat-Completions-compatible endpoint
    (OpenAI itself, Azure OpenAI, many self-hosted gateways). Activate with
    LLM_PROVIDER=openai_compatible plus LLM_API_KEY, LLM_MODEL_NAME, and
    (if not the public OpenAI API) LLM_BASE_URL.

    Alternative adapter for external or hosted OpenAI-compatible APIs."""

    def generate(self, prompt: str) -> LLMResponse:
        base_url = settings.llm_base_url or "https://api.openai.com/v1"
        url = f"{base_url.rstrip('/')}/chat/completions"
        headers = {
            "Authorization": f"Bearer {settings.llm_api_key}",
            "Content-Type": "application/json",
        }
        body = {
            "model": settings.llm_model_name,
            "messages": [{"role": "user", "content": prompt}],
        }
        start = time.monotonic()
        try:
            resp = httpx.post(
                url, json=body, headers=headers,
                timeout=settings.llm_timeout_ms / 1000.0,
            )
        except httpx.TimeoutException:
            raise LlmTimeoutError()
        except httpx.RequestError as e:
            raise LlmProviderError(f"Provider request failed: {e}")

        elapsed_ms = int((time.monotonic() - start) * 1000)

        if resp.status_code == 429:
            raise LlmRateLimitError()
        if resp.status_code >= 500:
            raise LlmProviderError(f"Provider returned {resp.status_code}")
        if resp.status_code == 400:
            # Many providers use 400 for content-policy rejections.
            raise LlmContentFilteredError()
        if resp.status_code != 200:
            raise LlmProviderError(f"Provider returned unexpected status {resp.status_code}")

        try:
            data = resp.json()
            text = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, ValueError) as e:
            raise LlmProviderError(f"Could not parse provider response: {e}")

        return LLMResponse(text=text, llm_ms=elapsed_ms)


class OllamaLLMClient(LLMClient):
    """Adapter for a local Ollama server running Qwen3 8B (or any configured model).
    Communicates via Ollama's HTTP API (/api/chat).

    Configuration:
      LLM_PROVIDER=ollama
      LLM_MODEL_NAME=qwen3:8b
      LLM_BASE_URL=http://localhost:11434
      LLM_TIMEOUT_MS=8000
      LLM_TEMPERATURE=0.2
      LLM_MAX_TOKENS=500
    """

    def generate(self, prompt: str) -> LLMResponse:
        base_url = settings.llm_base_url or "http://host.docker.internal:11434"
        model = settings.llm_model_name or "qwen3:8b"
        url = f"{base_url.rstrip('/')}/api/chat"

        payload = {
            "model": model,
            "messages": [{"role": "user", "content": prompt}],
            "stream": False,
            "options": {
                "temperature": settings.llm_temperature,
                "num_predict": settings.llm_max_tokens,
            },
        }

        start = time.monotonic()
        try:
            resp = httpx.post(
                url,
                json=payload,
                timeout=settings.llm_timeout_ms / 1000.0,
            )
        except httpx.TimeoutException:
            raise LlmTimeoutError()
        except httpx.ConnectError:
            raise LlmProviderError("Ollama server is unavailable")
        except httpx.RequestError as e:
            raise LlmProviderError(f"Ollama request failed: {e}")

        elapsed_ms = int((time.monotonic() - start) * 1000)

        if resp.status_code == 404:
            raise LlmProviderError(f"Model '{model}' not found in Ollama")
        if resp.status_code == 429:
            raise LlmRateLimitError()
        if resp.status_code != 200:
            raise LlmProviderError(f"Ollama returned HTTP {resp.status_code}")

        try:
            data = resp.json()
        except Exception as e:
            raise LlmProviderError(f"Malformed response from Ollama: {e}")

        if "message" in data and isinstance(data["message"], dict):
            text = data["message"].get("content", "")
        elif "response" in data:
            text = data.get("response", "")
        else:
            raise LlmProviderError("Malformed response from Ollama: missing message content")

        if not text or not text.strip():
            raise LlmProviderError("Ollama returned an empty response")

        return LLMResponse(text=text.strip(), llm_ms=elapsed_ms)


def get_llm_client() -> LLMClient:
    if settings.llm_provider == "ollama":
        return OllamaLLMClient()
    if settings.llm_provider == "openai_compatible":
        return OpenAICompatibleClient()
    # Mock fallback for automated tests and CI
    return MockLLMClient()

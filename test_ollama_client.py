import httpx
import pytest

from app.config import settings
from app.errors import LlmProviderError, LlmRateLimitError, LlmTimeoutError
from app.llm_client import OllamaLLMClient


def test_ollama_successful_response(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, json=None, headers=None, timeout=None):
        return httpx.Response(
            status_code=200,
            json={
                "model": "qwen3:8b",
                "message": {
                    "role": "assistant",
                    "content": "This is a verified answer. [chunk:c1]",
                },
                "done": True,
            },
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    resp = client.generate("User query prompt")
    assert resp.text == "This is a verified answer. [chunk:c1]"
    assert isinstance(resp.llm_ms, int)
    assert resp.llm_ms >= 0


def test_ollama_timeout(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        raise httpx.TimeoutException("Request timed out")

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmTimeoutError):
        client.generate("User query prompt")


def test_ollama_unavailable(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        raise httpx.ConnectError("Connection refused")

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmProviderError) as exc_info:
        client.generate("User query prompt")
    assert "unavailable" in str(exc_info.value).lower()


def test_ollama_malformed_response(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        # Missing 'message' or 'response'
        return httpx.Response(
            status_code=200,
            json={"unexpected_field": "some data"},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmProviderError) as exc_info:
        client.generate("User query prompt")
    assert "malformed" in str(exc_info.value).lower()


def test_ollama_empty_response(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        return httpx.Response(
            status_code=200,
            json={"message": {"role": "assistant", "content": "   \n\t  "}},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmProviderError) as exc_info:
        client.generate("User query prompt")
    assert "empty" in str(exc_info.value).lower()


def test_ollama_model_not_found(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        return httpx.Response(
            status_code=404,
            json={"error": "model 'qwen3:8b' not found"},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmProviderError) as exc_info:
        client.generate("User query prompt")
    assert "not found" in str(exc_info.value).lower()


def test_ollama_rate_limited(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        return httpx.Response(
            status_code=429,
            json={"error": "too many requests"},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmRateLimitError):
        client.generate("User query prompt")


def test_ollama_api_error_status(monkeypatch):
    client = OllamaLLMClient()

    def mock_post(url, **kwargs):
        return httpx.Response(
            status_code=500,
            json={"error": "internal error"},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    with pytest.raises(LlmProviderError) as exc_info:
        client.generate("User query prompt")
    assert "500" in str(exc_info.value)


def test_ollama_correct_model_base_url_and_parameter_mapping(monkeypatch):
    client = OllamaLLMClient()
    captured_requests = []

    def mock_post(url, json=None, headers=None, timeout=None):
        captured_requests.append({"url": url, "json": json, "timeout": timeout})
        return httpx.Response(
            status_code=200,
            json={"message": {"role": "assistant", "content": "Valid answer. [chunk:c1]"}},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    client.generate("Test prompt content")

    assert len(captured_requests) == 1
    req = captured_requests[0]
    expected_url = f"{settings.llm_base_url.rstrip('/')}/api/chat"
    assert req["url"] == expected_url

    payload = req["json"]
    assert payload["model"] == settings.llm_model_name
    assert payload["messages"] == [{"role": "user", "content": "Test prompt content"}]
    assert payload["stream"] is False

    options = payload["options"]
    assert options["temperature"] == settings.llm_temperature
    assert options["num_predict"] == settings.llm_max_tokens


def test_ollama_connectivity_host_docker_internal(monkeypatch):
    client = OllamaLLMClient()
    calls = []

    def mock_post(url, **kwargs):
        calls.append(url)
        return httpx.Response(
            status_code=200,
            json={"message": {"role": "assistant", "content": "Connected. [chunk:c1]"}},
            request=httpx.Request("POST", url),
        )

    monkeypatch.setattr(httpx, "post", mock_post)
    resp = client.generate("Ping")
    assert resp.text == "Connected. [chunk:c1]"
    assert calls[0].startswith("http://host.docker.internal:11434")


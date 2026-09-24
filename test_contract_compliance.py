# Contract tests for the Phase 5 -> Phase 6 boundary (spec §19A, contract
# §26). These validate the request against the exact OrchestratorContext
# shape Phase 5 sends, and the response against the exact envelope Phase
# 5's phase6_client.py parses.


def test_health(client):
    resp = client.get("/health")
    assert resp.status_code == 200
    assert resp.json() == {"status": "ok"}


def test_generate_success_shape(client, valid_headers, sample_context):
    resp = client.post("/internal/v1/llm/generate", json=sample_context, headers=valid_headers)
    assert resp.status_code == 200
    body = resp.json()

    assert body["success"] is True
    data = body["data"]
    assert data["query_id"] == "q1"
    assert isinstance(data["answer"], str)
    assert isinstance(data["sources"], list)
    assert isinstance(data["llm_ms"], int)

    validation = data["validation"]
    assert validation["groundedness"] in ("high", "medium", "low")
    assert isinstance(validation["unsupported_claims"], list)
    assert isinstance(validation["citation_coverage"], float)
    assert isinstance(validation["passed"], bool)

    meta = body["meta"]
    assert meta["request_id"] == "req-test-1"
    assert "timestamp" in meta

    if data["sources"]:
        source = data["sources"][0]
        assert set(source.keys()) == {"document_id", "chunk_id", "excerpt", "score", "page_or_location"}


def test_missing_internal_auth_rejected(client, sample_context):
    resp = client.post(
        "/internal/v1/llm/generate",
        json=sample_context,
        headers={"X-Request-ID": "req-2"},
    )
    assert resp.status_code == 401
    body = resp.json()
    assert body["success"] is False
    assert body["error"]["code"] == "UNAUTHENTICATED"


def test_wrong_internal_auth_rejected(client, sample_context):
    resp = client.post(
        "/internal/v1/llm/generate",
        json=sample_context,
        headers={"X-Request-ID": "req-3", "X-Internal-Auth": "wrong-token"},
    )
    assert resp.status_code == 401


def test_unknown_field_rejected(client, valid_headers, sample_context):
    bad_context = dict(sample_context)
    bad_context["unexpected_field"] = "should not be allowed"
    resp = client.post("/internal/v1/llm/generate", json=bad_context, headers=valid_headers)
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "VALIDATION_ERROR"


def test_empty_retrieved_chunks_rejected(client, valid_headers, sample_context):
    bad_context = dict(sample_context)
    bad_context["retrieved_chunks"] = []
    resp = client.post("/internal/v1/llm/generate", json=bad_context, headers=valid_headers)
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "VALIDATION_ERROR"


def test_invalid_role_rejected(client, valid_headers, sample_context):
    bad_context = dict(sample_context)
    bad_context["identity"] = dict(bad_context["identity"])
    bad_context["identity"]["roles"] = ["superadmin"]
    resp = client.post("/internal/v1/llm/generate", json=bad_context, headers=valid_headers)
    assert resp.status_code == 400


def test_generate_invalid_citation_triggers_fallback(client, valid_headers, sample_context):
    from app.llm_client import LLMClient, LLMResponse, get_llm_client
    from app.main import app
    from app.validation import FALLBACK_ANSWER

    class FakeClient(LLMClient):
        def generate(self, prompt: str) -> LLMResponse:
            return LLMResponse(text="Revenue grew a lot. [chunk:invented_id]", llm_ms=10)

    app.dependency_overrides[get_llm_client] = lambda: FakeClient()
    try:
        resp = client.post("/internal/v1/llm/generate", json=sample_context, headers=valid_headers)
        assert resp.status_code == 200
        body = resp.json()
        assert body["success"] is True
        data = body["data"]
        assert data["answer"] == FALLBACK_ANSWER
        assert data["sources"] == []
        assert data["validation"]["passed"] is False
        assert data["validation"]["groundedness"] == "low"
    finally:
        app.dependency_overrides.pop(get_llm_client, None)


def test_generate_one_time_regeneration_succeeds(client, valid_headers, sample_context):
    from app.llm_client import LLMClient, LLMResponse, get_llm_client
    from app.main import app

    calls = []

    class SelfCorrectingClient(LLMClient):
        def generate(self, prompt: str) -> LLMResponse:
            calls.append(prompt)
            if len(calls) == 1:
                # First attempt: uncited statement -> fails validation
                return LLMResponse(text="Revenue was 12.5 lakh rupees.", llm_ms=15)
            # Second attempt (regeneration): cited statement -> passes validation
            return LLMResponse(text="Revenue was 12.5 lakh rupees. [chunk:c1]", llm_ms=20)

    app.dependency_overrides[get_llm_client] = lambda: SelfCorrectingClient()
    try:
        resp = client.post("/internal/v1/llm/generate", json=sample_context, headers=valid_headers)
        assert resp.status_code == 200
        body = resp.json()
        assert body["success"] is True
        data = body["data"]
        assert data["answer"] == "Revenue was 12.5 lakh rupees."
        assert len(data["sources"]) == 1
        assert data["sources"][0]["chunk_id"] == "c1"
        assert data["validation"]["passed"] is True
        assert len(calls) == 2
        # Check strict regeneration prompt was passed on second attempt
        assert "IMPORTANT — your previous answer failed validation" in calls[1]
    finally:
        app.dependency_overrides.pop(get_llm_client, None)


def test_generate_regeneration_failure_returns_fallback(client, valid_headers, sample_context):
    from app.llm_client import LLMClient, LLMResponse, get_llm_client
    from app.main import app
    from app.validation import FALLBACK_ANSWER

    calls = []

    class AlwaysFailingClient(LLMClient):
        def generate(self, prompt: str) -> LLMResponse:
            calls.append(prompt)
            return LLMResponse(text="Uncited claim that always fails.", llm_ms=12)

    app.dependency_overrides[get_llm_client] = lambda: AlwaysFailingClient()
    try:
        resp = client.post("/internal/v1/llm/generate", json=sample_context, headers=valid_headers)
        assert resp.status_code == 200
        body = resp.json()
        assert body["success"] is True
        data = body["data"]
        assert data["answer"] == FALLBACK_ANSWER
        assert data["sources"] == []
        assert data["validation"]["passed"] is False
        assert len(calls) == 2  # Exactly 1 retry before fallback
    finally:
        app.dependency_overrides.pop(get_llm_client, None)


def test_generate_citation_to_unsent_chunk_fails(client, valid_headers, sample_context, monkeypatch):
    from app.config import settings
    from app.llm_client import LLMClient, LLMResponse, get_llm_client
    from app.main import app
    from app.validation import FALLBACK_ANSWER

    import dataclasses

    # Limit to 1 chunk in prompt so c2 is trimmed out by prompt budget
    new_settings = dataclasses.replace(settings, max_chunks_in_prompt=1)
    monkeypatch.setattr("app.prompt_builder.settings", new_settings)

    two_chunk_context = dict(sample_context)
    two_chunk_context["retrieved_chunks"] = [
        sample_context["retrieved_chunks"][0],  # chunk_id: "c1"
        {
            "chunk_id": "c2",
            "document_id": "d2",
            "text": "Unsent chunk text for c2.",
            "score": 0.85,
            "source_location": "page 6",
            "filename": "doc2.pdf",
        },
    ]

    calls = []

    class CitesUnsentChunkClient(LLMClient):
        def generate(self, prompt: str) -> LLMResponse:
            calls.append(prompt)
            # c1 is sent, but c2 is excluded from the prompt
            assert "chunk_id=\"c1\"" in prompt
            assert "chunk_id=\"c2\"" not in prompt

            # LLM cites the unsent chunk c2
            return LLMResponse(text="Claim citing unsent chunk. [chunk:c2]", llm_ms=10)

    app.dependency_overrides[get_llm_client] = lambda: CitesUnsentChunkClient()
    try:
        resp = client.post("/internal/v1/llm/generate", json=two_chunk_context, headers=valid_headers)
        assert resp.status_code == 200
        body = resp.json()
        assert body["success"] is True

        data = body["data"]
        # Validation must fail because c2 was not in chunks_used
        assert data["validation"]["passed"] is False
        assert data["validation"]["groundedness"] == "low"
        assert data["sources"] == []
        # Fallback answer must occur
        assert data["answer"] == FALLBACK_ANSWER

        # Proves that exactly one strict regeneration occurred before fallback
        assert len(calls) == 2
        assert "IMPORTANT — your previous answer failed validation" in calls[1]
    finally:
        app.dependency_overrides.pop(get_llm_client, None)


def test_generate_multiple_citations_in_response(client, valid_headers, sample_context):
    from app.llm_client import LLMClient, LLMResponse, get_llm_client
    from app.main import app

    two_chunk_context = dict(sample_context)
    two_chunk_context["retrieved_chunks"] = [
        sample_context["retrieved_chunks"][0],
        {
            "chunk_id": "c2",
            "document_id": "d2",
            "text": "Net margin was 18 percent.",
            "score": 0.88,
            "source_location": "page 7",
            "filename": "report.pdf",
        },
    ]

    class MultiCiteClient(LLMClient):
        def generate(self, prompt: str) -> LLMResponse:
            return LLMResponse(
                text="Revenue was 12.5 lakh rupees [chunk:c1] and margin was 18 percent [chunk:c2].",
                llm_ms=25,
            )

    app.dependency_overrides[get_llm_client] = lambda: MultiCiteClient()
    try:
        resp = client.post("/internal/v1/llm/generate", json=two_chunk_context, headers=valid_headers)
        assert resp.status_code == 200
        data = resp.json()["data"]
        assert data["validation"]["passed"] is True
        assert len(data["sources"]) == 2
        chunk_ids = [s["chunk_id"] for s in data["sources"]]
        assert "c1" in chunk_ids and "c2" in chunk_ids
    finally:
        app.dependency_overrides.pop(get_llm_client, None)


def test_generate_empty_answer_returns_fallback(client, valid_headers, sample_context):
    from app.llm_client import LLMClient, LLMResponse, get_llm_client
    from app.main import app
    from app.validation import FALLBACK_ANSWER

    class EmptyAnswerClient(LLMClient):
        def generate(self, prompt: str) -> LLMResponse:
            return LLMResponse(text="", llm_ms=5)

    app.dependency_overrides[get_llm_client] = lambda: EmptyAnswerClient()
    try:
        resp = client.post("/internal/v1/llm/generate", json=sample_context, headers=valid_headers)
        assert resp.status_code == 200
        data = resp.json()["data"]
        assert data["answer"] == FALLBACK_ANSWER
        assert data["sources"] == []
        assert data["validation"]["passed"] is False
    finally:
        app.dependency_overrides.pop(get_llm_client, None)



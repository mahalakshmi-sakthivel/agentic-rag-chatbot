# Phase 6 — LLM Integration & Response Generation

Internal service that Phase 5 calls once per query turn to generate the
final, validated answer. Not part of the public API — never reachable
through Phase 1 or the UI directly.

The current development/test configuration uses Ollama with Qwen3 8B (`qwen3:8b`).
The LLM provider and model remain configurable through the `LLMClient` abstraction and are not fixed as the final production choice. `MockLLMClient` is preserved for deterministic unit tests and CI.

## Endpoint

```
POST /internal/v1/llm/generate
Content-Type: application/json
X-Request-ID: <query_id>
X-Internal-Auth: <INTERNAL_SERVICE_TOKEN>
```

Request body: `OrchestratorContext` (see `app/schemas.py`) — the exact
shape defined in Phase 5's
`PHASE_5_AGENTIC_RAG_FINAL_PYTHON_SCOPE_v2.1.md` §19A.3 and implemented in
Phase 5's `app/schemas/models.py` / `app/clients/phase6_client.py`.

Response: standard success/error envelope carrying `Phase6Data` (`answer`,
`sources`, `validation`, `llm_ms`) — see §19A.4/§19A.5 for the exact shapes
and the error-code/HTTP-status table this service implements.

## Setting Up Ollama with Qwen3 8B

1. **Install Ollama**:
   Download and install Ollama from [ollama.com](https://ollama.com).

2. **Pull the Qwen3 8B model**:
   ```bash
   ollama pull qwen3:8b
   ```

3. **Start the Ollama server**:
   ```bash
   ollama serve
   ```
   By default, Ollama listens on `http://localhost:11434`.

## Environment Variables

For local development, copy `.env.example` to `.env` and configure the required values. The `.env` file is local-only and must not be committed or included in the final project package.

```ini
INTERNAL_SERVICE_TOKEN=change-me-to-a-random-secret

# LLM provider settings
LLM_PROVIDER=ollama
LLM_MODEL_NAME=qwen3:8b
LLM_BASE_URL=http://host.docker.internal:11434
LLM_TIMEOUT_MS=8000
LLM_TEMPERATURE=0.2
LLM_MAX_TOKENS=500
LLM_API_KEY=

# Budgeting and validation
MAX_CONTEXT_TOKENS=6000
MAX_CHUNKS_IN_PROMPT=20
MIN_CITATION_COVERAGE=0.6
ALLOW_REGENERATION=true
```

## Deployment Architecture & Docker-to-Host Networking

- **Ollama**: Runs natively on the host server (outside Docker) to take direct advantage of host GPUs and system accelerators.
  Ensure Ollama is accessible from container bridges:
  ```bash
  OLLAMA_HOST=0.0.0.0 ollama serve
  ```
- **Phase 6 Service**: Runs containerized inside Docker.
- **Networking**: `docker-compose.yml` configures:
  ```yaml
  extra_hosts:
    - "host.docker.internal:host-gateway"
  ```
  The Phase 6 container connects to the host's Ollama instance using:
  ```
  LLM_BASE_URL=http://host.docker.internal:11434
  ```
  *(Note: If running Phase 6 directly on the host without Docker, `LLM_BASE_URL` can be set to `http://localhost:11434`)*

## Running Locally

```bash
pip install -r requirements.txt
cp .env.example .env
uvicorn app.main:app --host 0.0.0.0 --port 8006 --env-file .env --reload
```

## Running with Docker

```bash
docker compose up --build
```

Exposes port `8006`, matching Phase 5's default `PHASE6_BASE_URL=http://localhost:8006`.

## Running Tests

Unit and contract tests use mocked clients or `MockLLMClient` and do not require a live Ollama server:

```bash
pip install -r requirements.txt
pytest -v
```

## What's Implemented

- **Ollama LLM Integration**: `OllamaLLMClient` calls `/api/chat` with configurable model (`qwen3:8b`), temperature (`0.2`), max tokens (`500`), and connection/timeout handling.
- **Provider-Agnostic Abstraction**: `LLMClient` interface allows swapping between `OllamaLLMClient`, `OpenAICompatibleClient`, and `MockLLMClient` via `LLM_PROVIDER`.
- **Context Validation**: Pydantic schema with `extra="forbid"` to catch schema drift.
- **Internal Service Auth**: Constant-time `X-Internal-Auth` verification.
- **Prompt Builder with Trust Boundary**: Context chunks wrapped in `<context>` blocks, treated as untrusted data with prompt injection defense.
- **Deterministic Citation & Coverage Validation**: Deterministic citation and evidence-coverage validation against the supplied retrieved chunks. Verifies that every factual claim cites only chunk IDs actually supplied to the LLM (`chunks_used`), without claiming perfect semantic or factual verification.
- **One-Time Regeneration & Fallback**: Automatically retries failed validation once with a stricter prompt; returns a safe fallback response if regeneration fails.

## Still Open / TBD (confirm before treating as final)

1. The Phase 5→6 handoff spec (§19A in the Phase 5 project document) is marked [PROPOSED] — it has not yet been merged into TECHNICAL_CONTRACT.md as a ratified Section 27 amendment. This service is built against it because Phase 5's code already assumes it.
2. LLM_CONTENT_FILTERED's HTTP status code is listed as TBD in that same spec. This implementation currently returns 422 (see app/errors.py, LlmContentFilteredError) as a reasoned default — it has not been formally confirmed with the team.
3. MIN_CITATION_COVERAGE (default 0.6) and the high/medium/low groundedness thresholds in app/validation.py are reasonable defaults chosen during implementation, not numbers specified anywhere in the contract. Confirm with the team before relying on them for a graded milestone.

LLM provider default is Ollama + Qwen3 8B (qwen3:8b) — confirm this is the team's agreed choice for §31 Item 8; the LLMClient interface in app/llm_client.py allows swapping providers without other code changes if not.


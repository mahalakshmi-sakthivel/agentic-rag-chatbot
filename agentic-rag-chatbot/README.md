# Phase 5: Agentic RAG Orchestrator

This module implements Phase 5 (Agentic RAG Orchestrator) of the Plug-and-Play Agentic RAG Chatbot. It acts as the central intelligence hub, taking normalized user queries, determining intent, planning a sequence of tool calls, evaluating evidence, and gating the handoff to Phase 6 (LLM Integration).

## Core Features

- **Strict Validations & Limits:** Enforces hard boundaries on context sizes (`MAX_TOP_K`), iteration steps (`MAX_TOOL_CALLS`), and similarity thresholds (`MIN_RELEVANCE_SCORE`).
- **Tools Registry:** Supports secure, identity-aware Vector Search integrations (Phase 4), Mock Structured Query backends, and a strictly bounded Python AST-based Calculator.
- **Handoff Gate:** Evaluates the gathered `OrchestratorContext` and blocks the LLM from hallucinating by short-circuiting to a "Clarification" or "Failure" state if insufficient evidence is gathered.
- **Resilient Phase 6 Client:** A highly optimized HTTPX client with precise status mapping and exact-retry logic for connection errors.
- **Structured JSON Logging:** Outputs fully parsed, PII-redacted JSON summary telemetry for every turn.

## Installation & Setup

1. **Prerequisites:** Python 3.11+
2. **Install dependencies:**
   ```bash
   pip install .
   ```
   *(For development/testing, install `pip install .[test]`)*
3. **Environment Configuration:**
   Copy `.env.example` to `.env` and fill out your local URLs and keys.

## Testing

All constraints, logic gates, error responses, and mock tool workflows are thoroughly tested with `pytest`.

```bash
set PYTHONPATH=.
pytest tests/
```

## Docker

A lightweight `Dockerfile` is provided for immediate containerized deployment, running via the `phase5-agent` entrypoint.

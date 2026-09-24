# app/services.py
#
# Orchestrates the full Phase 6 internal flow (Part 5 of the Phase 6
# planning discussion):
#
#   Context -> Validation -> Prompt Builder -> Token Budget -> LLMClient
#   -> LLM Provider -> Raw Response -> Response Parser -> Citation Mapping
#   -> Response Validation -> (regenerate once if failed) -> Phase6Data
#
# Context validation itself (schema shape) is handled by FastAPI/Pydantic
# at the route boundary (app/main.py) — by the time generate() is called,
# `context` is already a valid OrchestratorContext.
from __future__ import annotations

from .citation import build_sources, extract_sentence_citations, strip_all_citations
from .config import settings
from .errors import ValidationError
from .llm_client import LLMClient
from .prompt_builder import build_prompt
from .schemas import OrchestratorContext, Phase6Data
from .validation import FALLBACK_ANSWER, compute_validation, fallback_validation


def generate_answer(context: OrchestratorContext, llm_client: LLMClient) -> Phase6Data:
    if not context.retrieved_chunks:
        # Contract requirement: an empty evidence set should never reach the
        # LLM as if it had context. Phase 5's own spec (§19A.2) says it
        # won't send one, but Phase 6 must not assume upstream is bug-free.
        raise ValidationError(
            "retrieved_chunks must not be empty",
            details={"field": "retrieved_chunks"},
        )

    total_llm_ms = 0

    def run(strict: bool) -> tuple[Phase6Data, bool]:
        nonlocal total_llm_ms
        prompt, chunks_used = build_prompt(context, strict=strict)
        llm_response = llm_client.generate(prompt)
        total_llm_ms += llm_response.llm_ms

        known_chunk_ids = {c.chunk_id for c in chunks_used}
        sentences = extract_sentence_citations(llm_response.text, known_chunk_ids)
        validation = compute_validation(sentences)
        visible_answer = strip_all_citations(llm_response.text)
        sources = build_sources(sentences, chunks_used)

        return Phase6Data(
            query_id=context.query_id,
            answer=visible_answer,
            sources=sources,
            validation=validation,
            llm_ms=total_llm_ms,
        ), validation.passed

    result, passed = run(strict=False)

    if not passed and settings.allow_regeneration:
        # Task 11: one-time stricter regeneration on validation failure
        # (contract §17.2 offers exactly two choices — regenerate once, or
        # fall back — a second LLM judge is explicitly optional, not used
        # here).
        result, passed = run(strict=True)

    if not passed:
        # Fallback / insufficient-information response (contract §17.2).
        result = Phase6Data(
            query_id=context.query_id,
            answer=FALLBACK_ANSWER,
            sources=[],
            validation=fallback_validation(),
            llm_ms=total_llm_ms,
        )

    return result

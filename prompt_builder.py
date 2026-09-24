# app/prompt_builder.py
#
# Builds the LLM request from OrchestratorContext (Part 6 of the Phase 6
# planning discussion). Responsibilities kept deliberately narrow:
#
#   System Prompt + Retrieved Context + Conversation History + User Query
#   = LLM Request
#
# Trust boundary (contract §19/§22, Phase 5 spec §22): the system prompt is
# the ONLY trusted instruction channel. Everything under "Retrieved context"
# is untrusted data from documents a user uploaded — it is delimited and
# never treated as instructions, and the system prompt explicitly tells the
# model to ignore any instruction-like text found inside it.
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Tuple

from .config import settings
from .schemas import Chunk, OrchestratorContext

_PROMPTS_DIR = Path(__file__).resolve().parent.parent / "prompts"


def _load_prompt(filename: str) -> str:
    path = _PROMPTS_DIR / filename
    return path.read_text(encoding="utf-8")


SYSTEM_PROMPT = _load_prompt("system_v1.txt")
STRICT_REGENERATION_SUFFIX = "\n\n" + _load_prompt("regeneration_v1.txt").strip()


def _approx_tokens(text: str) -> int:
    # Provider-agnostic approximation (chars/4) — deliberately not a
    # model-specific tokenizer, since the tokenizer is provider-dependent
    # and the provider is still TBD (contract §31 Item 8). Swap for a real
    # tokenizer once a provider is chosen.
    return max(1, len(text) // 4)


def select_chunks_within_budget(
    chunks: List[Chunk],
    query: str = "",
    history_text: str = "",
) -> List[Chunk]:
    """Token/context-window budgeting (Part 6 / contract §16.2).

    Phase 5 already dedupes, score-filters, and orders `retrieved_chunks`
    (see Phase 5 context/builder.py) — this does not re-rank. It only trims
    for the LLM's context window, stopping once the budget or the configured
    max chunk count is reached.
    """
    selected: List[Chunk] = []
    used_tokens = (
        _approx_tokens(SYSTEM_PROMPT)
        + _approx_tokens(query)
        + _approx_tokens(history_text)
    )

    for chunk in chunks[: settings.max_chunks_in_prompt]:
        chunk_tokens = _approx_tokens(chunk.text)
        if used_tokens + chunk_tokens > settings.max_context_tokens and selected:
            # Always keep at least one chunk even if it alone exceeds the
            # budget, so a single large chunk doesn't collapse to zero context.
            break
        selected.append(chunk)
        used_tokens += chunk_tokens

    return selected


def format_context_block(chunks: List[Chunk]) -> str:
    parts = []
    for c in chunks:
        attrs = [f'chunk_id="{c.chunk_id}"', f'document_id="{c.document_id}"']
        if c.source_location is not None:
            attrs.append(f'source="{c.source_location}"')
        if c.filename is not None:
            attrs.append(f'filename="{c.filename}"')
        attrs_str = " ".join(attrs)
        parts.append(f"<context {attrs_str}>\n{c.text}\n</context>")
    return "\n\n".join(parts)


def format_conversation_history(history: List[Dict[str, Any]] | None) -> str:
    if not history:
        return ""
    lines = []
    for turn in history:
        role = turn.get("role", "user")
        content = turn.get("content", "")
        lines.append(f"{role}: {content}")
    return "\n".join(lines)


def build_prompt(context: OrchestratorContext, strict: bool = False) -> Tuple[str, List[Chunk]]:
    """Returns (full_prompt_text, chunks_actually_used).

    `chunks_actually_used` is what citation mapping and validation check
    against — it may be a subset of context.retrieved_chunks if token
    budgeting trimmed some out.
    """
    query = (
        context.refined_query.strip()
        if context.refined_query and context.refined_query.strip()
        else context.original_query
    )
    history_text = format_conversation_history(context.conversation_history)
    used_chunks = select_chunks_within_budget(
        context.retrieved_chunks,
        query=query,
        history_text=history_text,
    )

    sections = [SYSTEM_PROMPT]
    if strict:
        sections.append(STRICT_REGENERATION_SUFFIX)

    if history_text:
        sections.append(f"\nConversation so far:\n{history_text}")

    sections.append(f"\n<context_blocks>\n{format_context_block(used_chunks)}\n</context_blocks>")
    sections.append(f"\nQuestion: {query}\n\nAnswer:")

    return "\n".join(sections), used_chunks

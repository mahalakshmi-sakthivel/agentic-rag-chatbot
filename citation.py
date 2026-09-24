# app/citation.py
#
# Citation extraction and source mapping (Part 8 of the Phase 6 planning
# discussion). The model is prompted (see prompt_builder.py) to tag every
# supported sentence with [chunk:<chunk_id>]. This module:
#   1. splits the answer into sentences,
#   2. extracts citation tags per sentence,
#   3. verifies each cited chunk_id actually exists in the chunks that were
#      sent to the LLM (deterministic consistency check — contract requires
#      Phase 6 to verify a cited chunk_id actually exists in retrieved
#      context, not merely trust the model's output),
#   4. strips the tags from the visible answer text,
#   5. builds the Phase6Source list from only the chunks that were actually
#      cited (never inventing sources for uncited chunks).
from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Dict, List

from .schemas import Chunk, Phase6Source

CITATION_RE = re.compile(r"\[chunk:\s*([^\s\]]+)\s*\]")
SENTENCE_WITH_TRAILING_CITATIONS_RE = re.compile(
    r"(.+?[.!?](?!\d))((?:\s*\[chunk:\s*[^\s\]]+\s*\])*)",
    re.DOTALL,
)


@dataclass
class SentenceCitations:
    text: str  # sentence with citation tags stripped
    cited_chunk_ids: List[str]
    has_valid_citation: bool  # at least one cited id exists in known_chunks


def extract_sentence_citations(answer_text: str, known_chunk_ids: set) -> List[SentenceCitations]:
    text = answer_text.strip()
    if not text:
        return []

    results: List[SentenceCitations] = []
    matched_spans: List[tuple] = []

    for match in SENTENCE_WITH_TRAILING_CITATIONS_RE.finditer(text):
        sentence_part, tags_part = match.group(1), match.group(2)
        matched_spans.append(match.span())

        cited_ids = CITATION_RE.findall(sentence_part + tags_part)
        clean_text = CITATION_RE.sub("", sentence_part).strip()
        valid_ids = [cid for cid in cited_ids if cid in known_chunk_ids]
        has_valid_citation = len(cited_ids) > 0 and len(valid_ids) == len(cited_ids)

        results.append(
            SentenceCitations(
                text=clean_text,
                cited_chunk_ids=cited_ids,
                has_valid_citation=has_valid_citation,
            )
        )

    # Any leftover text after the last matched sentence (e.g. no
    # terminal punctuation at all) still needs to be checked — don't
    # silently drop it.
    consumed_end = matched_spans[-1][1] if matched_spans else 0
    trailing = text[consumed_end:].strip()
    if trailing:
        cited_ids = CITATION_RE.findall(trailing)
        clean_text = CITATION_RE.sub("", trailing).strip()
        if clean_text:
            valid_ids = [cid for cid in cited_ids if cid in known_chunk_ids]
            has_valid_citation = len(cited_ids) > 0 and len(valid_ids) == len(cited_ids)
            results.append(
                SentenceCitations(
                    text=clean_text,
                    cited_chunk_ids=cited_ids,
                    has_valid_citation=has_valid_citation,
                )
            )

    return results


def strip_all_citations(answer_text: str) -> str:
    return CITATION_RE.sub("", answer_text).strip()


def build_sources(
    sentences: List[SentenceCitations],
    chunks_used: List[Chunk],
) -> List[Phase6Source]:
    """Builds the Phase6Source list from only the chunk_ids that were
    actually cited (and that are real, known chunks) — never every chunk
    that was merely sent to the LLM."""
    chunk_by_id: Dict[str, Chunk] = {c.chunk_id: c for c in chunks_used}
    cited_ids_in_order: List[str] = []
    seen = set()

    for sentence in sentences:
        for cid in sentence.cited_chunk_ids:
            if cid in chunk_by_id and cid not in seen:
                cited_ids_in_order.append(cid)
                seen.add(cid)

    sources: List[Phase6Source] = []
    for cid in cited_ids_in_order:
        chunk = chunk_by_id[cid]
        sources.append(
            Phase6Source(
                document_id=chunk.document_id,
                chunk_id=chunk.chunk_id,
                excerpt=chunk.text[:280],
                score=chunk.score,
                page_or_location=chunk.source_location,
            )
        )
    return sources

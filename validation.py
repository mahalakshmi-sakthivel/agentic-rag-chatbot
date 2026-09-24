# app/validation.py
#
# Response validation (Part 8 of the Phase 6 planning discussion):
# groundedness, unsupported claims, citation coverage, and the deterministic
# citation/source consistency check. This is a fully deterministic,
# rule-based validator — the contract treats a second LLM-as-judge as
# optional, not mandatory (TECHNICAL_CONTRACT §17.2: "must not be the sole
# authority"), so no second model call happens here.
from __future__ import annotations

from typing import List

from .citation import SentenceCitations
from .config import settings
from .schemas import Phase6Validation


def compute_validation(sentences: List[SentenceCitations]) -> Phase6Validation:
    if not sentences:
        return Phase6Validation(
            groundedness="low",
            unsupported_claims=[],
            citation_coverage=0.0,
            passed=False,
        )

    total = len(sentences)
    supported = sum(1 for s in sentences if s.has_valid_citation)
    coverage = supported / total

    unsupported_claims = [s.text for s in sentences if not s.has_valid_citation and s.text]

    if coverage >= 0.9:
        groundedness = "high"
    elif coverage >= settings.min_citation_coverage:
        groundedness = "medium"
    else:
        groundedness = "low"

    passed = coverage >= settings.min_citation_coverage and len(unsupported_claims) == 0

    return Phase6Validation(
        groundedness=groundedness,
        unsupported_claims=unsupported_claims,
        citation_coverage=round(coverage, 4),
        passed=passed,
    )


FALLBACK_ANSWER = (
    "The provided documents do not contain enough information to answer "
    "this question with confidence."
)


def fallback_validation() -> Phase6Validation:
    return Phase6Validation(
        groundedness="low",
        unsupported_claims=[],
        citation_coverage=0.0,
        passed=False,
    )

from __future__ import annotations

import re

from rapidfuzz import fuzz, process


_NOISE = re.compile(
    r"\b(the|a|an|disc|disk|cd|dvd|vol\.?|volume)\b"
    r"|\s*[\(\[].*?[\)\]]"  # parenthetical tags like (USA), [NTSC]
    r"|[^\w\s]",            # punctuation
    re.IGNORECASE,
)


def _normalize(title: str) -> str:
    return _NOISE.sub(" ", title).lower().split()


def _norm_str(title: str) -> str:
    return " ".join(_normalize(title))


def score(query: str, candidate: str) -> float:
    """Return 0–100 similarity between two titles."""
    q = _norm_str(query)
    c = _norm_str(candidate)
    token_ratio = fuzz.token_sort_ratio(q, c)
    partial_ratio = fuzz.partial_ratio(q, c)
    return max(token_ratio, partial_ratio)


def best_match(
    query: str,
    candidates: list[str],
    threshold: float = 60.0,
) -> list[tuple[str, float]]:
    """Return candidates above threshold sorted by score descending."""
    results = process.extract(
        _norm_str(query),
        [_norm_str(c) for c in candidates],
        scorer=fuzz.token_sort_ratio,
        limit=None,
    )
    # map back to original candidates
    scored = [
        (candidates[idx], sim)
        for _, sim, idx in results
        if sim >= threshold
    ]
    return sorted(scored, key=lambda x: x[1], reverse=True)

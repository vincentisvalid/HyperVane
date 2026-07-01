import pytest
from scraper.matchers.title import score, best_match


def test_score_exact():
    assert score("Metal Gear Solid 4", "Metal Gear Solid 4") == 100


def test_score_normalizes_articles():
    assert score("The Last of Us", "Last of Us, The") > 90


def test_score_ignores_region_tags():
    s = score("Gran Turismo 4", "Gran Turismo 4 (USA)")
    assert s > 85


def test_score_partial():
    s = score("God of War", "God of War III")
    assert 0 < s < 100


def test_best_match_returns_sorted():
    candidates = [
        "Uncharted 2: Among Thieves",
        "Uncharted: Drake's Fortune",
        "Killzone 2",
    ]
    results = best_match("Uncharted 2", candidates)
    assert results
    assert "Uncharted 2" in results[0][0]


def test_best_match_threshold_filters():
    candidates = ["Gran Turismo 5", "Forza Motorsport"]
    results = best_match("Totally Unrelated Game", candidates, threshold=80)
    assert results == []

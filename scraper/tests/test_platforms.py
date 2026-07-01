import pytest
from scraper.platforms import get, PLATFORMS


def test_all_platforms_registered():
    for pid in ("ps1", "ps2", "ps3", "ps4", "psp"):
        p = get(pid)
        assert p.id == pid
        assert p.romsfun_slug
        assert p.formats


def test_unknown_platform_raises():
    with pytest.raises(ValueError, match="Unknown platform"):
        get("atari2600")


def test_platform_ids_match_keys():
    for key, pinfo in PLATFORMS.items():
        assert key == pinfo.id


def test_romsfun_slugs_unique():
    slugs = [p.romsfun_slug for p in PLATFORMS.values()]
    assert len(slugs) == len(set(slugs))

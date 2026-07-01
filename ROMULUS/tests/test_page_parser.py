from romulus.sources.page_parser import parse_pspkg_detail, parse_romsfun_detail
from tests.fixtures.detail_pages import ROMSFUN_HTML, PSPKG_HTML


def test_parse_romsfun_detail():
    meta = parse_romsfun_detail(
        ROMSFUN_HTML,
        "https://romsfun.com/roms/playstation/devil-may-cry.html",
    )
    assert meta.title == "Devil May Cry"
    assert "demon hunter" in (meta.description or "").lower()
    assert meta.publisher == "Capcom"


def test_parse_pspkg_detail():
    meta = parse_pspkg_detail(
        PSPKG_HTML,
        "https://pspkg.com/ps5/elden-ring-1-7577.html",
    )
    assert "Elden Ring" in meta.title
    assert meta.cover_url is not None
    assert meta.download_page_url is not None

from scraper.sources.page_parser import parse_detail_page
from tests.fixtures.detail_page import SAMPLE_DETAIL_HTML


def test_parse_detail_page_extracts_core_fields():
    meta = parse_detail_page(
        SAMPLE_DETAIL_HTML,
        "https://romsfun.com/roms/playstation-4/devil-may-cry-5-id2.html",
    )

    assert meta.title == "Devil May Cry 5"
    assert "Devil you know" in (meta.description or "")
    assert meta.cover_url == "https://romsfun.com/covers/dmc5.jpg"
    assert meta.publisher == "Capcom"
    assert meta.developer == "Capcom"
    assert meta.release_date == "March 8, 2019"
    assert meta.region == "USA"
    assert meta.rating == 3.9
    assert meta.review_count == 35
    assert meta.download_count == 4754
    assert meta.size_hint == "43.85 GB"
    assert meta.download_page_url is not None
    assert "/download/" in meta.download_page_url

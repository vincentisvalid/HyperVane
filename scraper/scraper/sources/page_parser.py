from __future__ import annotations

import re
from dataclasses import dataclass, field
from urllib.parse import urljoin

from bs4 import BeautifulSoup, Tag


@dataclass
class PageMetadata:
    title: str
    description: str | None = None
    cover_url: str | None = None
    publisher: str | None = None
    developer: str | None = None
    release_date: str | None = None
    region: str | None = None
    genres: list[str] = field(default_factory=list)
    rating: float | None = None
    review_count: int | None = None
    download_count: int | None = None
    size_hint: str | None = None
    screenshot_urls: list[str] = field(default_factory=list)
    download_page_url: str | None = None


def parse_detail_page(html: str, page_url: str) -> PageMetadata:
    soup = BeautifulSoup(html, "html.parser")
    base = page_url.rsplit("/", 1)[0] + "/"

    title = _text(soup.select_one("h1")) or _meta(soup, "og:title") or "Unknown"
    title = re.sub(r"\s*-\s*PS\d.*$", "", title, flags=re.I).strip()

    description = _section_text(soup, "Description")
    if not description:
        description = _meta(soup, "og:description")

    cover = _meta(soup, "og:image")
    if not cover:
        cover = _first_image(soup, page_url)

    publisher, developer, release_date, region = _table_fields(soup)
    genres = _genres(soup)
    rating, reviews = _rating(soup)
    downloads, size_hint = _stats_line(soup)
    screenshots = _screenshots(soup, page_url)
    download_page = _download_page(soup, page_url)

    return PageMetadata(
        title=title,
        description=description,
        cover_url=cover,
        publisher=publisher,
        developer=developer,
        release_date=release_date,
        region=region,
        genres=genres,
        rating=rating,
        review_count=reviews,
        download_count=downloads,
        size_hint=size_hint,
        screenshot_urls=screenshots,
        download_page_url=download_page,
    )


def parse_listing_links(html: str, slug: str, base_url: str) -> list[str]:
    soup = BeautifulSoup(html, "html.parser")
    links: list[str] = []
    seen: set[str] = set()

    for a in soup.select("a[href*='/roms/']"):
        href = a.get("href", "")
        if not href.endswith(".html"):
            continue
        if slug not in href:
            continue
        full = urljoin(base_url, href)
        if full in seen:
            continue
        seen.add(full)
        links.append(full)

    return links


def parse_search_results(html: str, slug: str, base_url: str) -> list[tuple[str, str]]:
    """Return (title, detail_url) pairs from a search/listing page."""
    soup = BeautifulSoup(html, "html.parser")
    results: list[tuple[str, str]] = []
    seen: set[str] = set()

    cards = soup.select(
        "article, .search-result-item, .col-item, .game-card, div[id*='post-']"
    )
    if not cards:
        cards = soup.select("a[href*='/roms/']")

    for card in cards:
        link = card if card.name == "a" else card.select_one("a[href*='/roms/']")
        if not link:
            continue
        href = urljoin(base_url, link.get("href", ""))
        if slug not in href or not href.endswith(".html"):
            continue
        if href in seen:
            continue
        seen.add(href)

        title = _card_title(card, link)
        if title:
            results.append((title, href))

    return results


def _text(node: Tag | None) -> str | None:
    if not node:
        return None
    text = node.get_text(" ", strip=True)
    return text or None


def _meta(soup: BeautifulSoup, prop: str) -> str | None:
    tag = soup.select_one(f'meta[property="{prop}"], meta[name="{prop}"]')
    if tag and tag.get("content"):
        return tag["content"].strip()
    return None


def _section_text(soup: BeautifulSoup, heading: str) -> str | None:
    for tag in soup.find_all(["h2", "h3", "h4"]):
        if tag.get_text(strip=True).lower() == heading.lower():
            parts: list[str] = []
            for sib in tag.find_next_siblings():
                if sib.name in ("h2", "h3", "h4", "hr"):
                    break
                if sib.name in ("p", "div"):
                    text = sib.get_text(" ", strip=True)
                    if text:
                        parts.append(text)
            if parts:
                return "\n\n".join(parts)
    return None


def _table_fields(soup: BeautifulSoup) -> tuple[str | None, str | None, str | None, str | None]:
    publisher = developer = release_date = region = None
    for row in soup.select("table tr"):
        cells = row.find_all(["th", "td"])
        if len(cells) < 2:
            continue
        label = cells[0].get_text(" ", strip=True).lower()
        value = cells[1].get_text(" ", strip=True)
        if not value:
            continue
        if label == "publisher":
            publisher = value
        elif label == "developer":
            developer = value
        elif label in ("release date", "released"):
            release_date = value
        elif label == "region":
            region = value
    return publisher, developer, release_date, region


def _genres(soup: BeautifulSoup) -> list[str]:
    genres: list[str] = []
    for tag in soup.select("a[href*='/genre/'], a[href*='/genres/']"):
        text = tag.get_text(strip=True)
        if text and text not in genres:
            genres.append(text)
    return genres


def _rating(soup: BeautifulSoup) -> tuple[float | None, int | None]:
    text = soup.get_text(" ", strip=True)
    m = re.search(r"(\d+(?:\.\d+)?)\s*\(\s*(\d+)\s*reviews?\s*\)", text, re.I)
    if not m:
        return None, None
    return float(m.group(1)), int(m.group(2))


def _stats_line(soup: BeautifulSoup) -> tuple[int | None, str | None]:
    text = soup.get_text(" ", strip=True)
    downloads = None
    size_hint = None

    dm = re.search(r"([\d,]+)\s+downloads?", text, re.I)
    if dm:
        downloads = int(dm.group(1).replace(",", ""))

    sm = re.search(r"([\d.]+\s*(?:GB|MB|KB))", text, re.I)
    if sm:
        size_hint = sm.group(1)

    return downloads, size_hint


def _first_image(soup: BeautifulSoup, page_url: str) -> str | None:
    for img in soup.select("img[src]"):
        src = img.get("src", "")
        if not src or "logo" in src.lower() or "icon" in src.lower():
            continue
        return urljoin(page_url, src)
    return None


def _screenshots(soup: BeautifulSoup, page_url: str) -> list[str]:
    urls: list[str] = []
    for img in soup.select(
        ".screenshot img, .gallery img, .swiper img, figure img, .entry-content img"
    ):
        src = img.get("src") or img.get("data-src")
        if not src:
            continue
        full = urljoin(page_url, src)
        if full not in urls:
            urls.append(full)
    return urls[:6]


def _download_page(soup: BeautifulSoup, page_url: str) -> str | None:
    for a in soup.select("a[href*='/download/']"):
        href = a.get("href", "")
        if "/download/" in href:
            return urljoin(page_url, href)
    return None


def _card_title(card: Tag, link: Tag) -> str:
    for sel in ("h3 a", "h4 a", "h2 a", "h3", "h4", "h2"):
        node = card.select_one(sel)
        if node:
            text = node.get_text(strip=True)
            if text:
                return text
    return link.get_text(strip=True)

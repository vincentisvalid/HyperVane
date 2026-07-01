from __future__ import annotations

import re
from dataclasses import dataclass, field
from urllib.parse import urljoin

from bs4 import BeautifulSoup, Tag


@dataclass
class ParsedPage:
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
    file_urls: list[str] = field(default_factory=list)


def parse_romsfun_detail(html: str, page_url: str) -> ParsedPage:
    soup = BeautifulSoup(html, "html.parser")
    title = _text(soup.select_one("h1")) or _meta(soup, "og:title") or "Unknown"
    title = re.sub(r"\s*-\s*PS\d.*$", "", title, flags=re.I).strip()

    return ParsedPage(
        title=title,
        description=_section_text(soup, "Description") or _meta(soup, "og:description"),
        cover_url=_meta(soup, "og:image") or _first_image(soup, page_url),
        publisher=_table_value(soup, "publisher"),
        developer=_table_value(soup, "developer"),
        release_date=_table_value(soup, "release date"),
        region=_table_value(soup, "region"),
        genres=_genre_links(soup),
        rating=_rating(soup)[0],
        review_count=_rating(soup)[1],
        download_count=_stats(soup)[0],
        size_hint=_stats(soup)[1],
        screenshot_urls=_screenshots(soup, page_url),
        download_page_url=_first_href(soup, "/download/", page_url),
    )


def parse_romsfun_links(html: str, slug: str, base_url: str) -> list[str]:
    soup = BeautifulSoup(html, "html.parser")
    links: list[str] = []
    seen: set[str] = set()
    for a in soup.select("a[href*='/roms/']"):
        href = a.get("href", "")
        if slug in href and href.endswith(".html"):
            full = urljoin(base_url, href)
            if full not in seen:
                seen.add(full)
                links.append(full)
    return links


def parse_romsfun_search(html: str, slug: str, base_url: str) -> list[tuple[str, str]]:
    soup = BeautifulSoup(html, "html.parser")
    results: list[tuple[str, str]] = []
    seen: set[str] = set()
    for a in soup.select("a[href*='/roms/']"):
        href = urljoin(base_url, a.get("href", ""))
        if slug not in href or not href.endswith(".html") or href in seen:
            continue
        seen.add(href)
        title = a.get_text(" ", strip=True) or href.rsplit("/", 1)[-1]
        results.append((title, href))
    return results


def parse_pspkg_detail(html: str, page_url: str) -> ParsedPage:
    soup = BeautifulSoup(html, "html.parser")
    title = (
        _text(soup.select_one("h1"))
        or _text(soup.select_one(".game-title"))
        or _meta(soup, "og:title")
        or "Unknown"
    )
    title = re.sub(r"\s*-\s*PSPKG.*$", "", title, flags=re.I).strip()

    description = (
        _section_text(soup, "Description")
        or _text(soup.select_one(".description, .game-description, #description"))
        or _meta(soup, "og:description")
    )

    file_urls = _pkg_links(soup, page_url)
    download_page = _first_href(soup, "/download", page_url) or page_url

    return ParsedPage(
        title=title,
        description=description,
        cover_url=_meta(soup, "og:image") or _hero_image(soup, page_url),
        publisher=_label_value(soup, "publisher"),
        developer=_label_value(soup, "developer"),
        release_date=_label_value(soup, "release"),
        genres=_genre_links(soup),
        rating=_rating(soup)[0],
        download_count=_stats(soup)[0],
        size_hint=_stats(soup)[1],
        screenshot_urls=_screenshots(soup, page_url),
        download_page_url=download_page,
        file_urls=file_urls,
    )


def parse_pspkg_links(html: str, platform: str, base_url: str) -> list[tuple[str, str]]:
    soup = BeautifulSoup(html, "html.parser")
    prefix = f"/{platform.lower()}/"
    results: list[tuple[str, str]] = []
    seen: set[str] = set()

    for a in soup.select("a[href]"):
        href = a.get("href", "")
        if not href.startswith(prefix) and f"{prefix}" not in href:
            continue
        if not href.endswith(".html"):
            continue
        full = urljoin(base_url, href)
        if full in seen:
            continue
        seen.add(full)
        title = a.get_text(" ", strip=True) or full.rsplit("/", 1)[-1].replace(".html", "")
        if title:
            results.append((title, full))

    return results


def extract_file_url(html: str, page_url: str, extensions: set[str]) -> str | None:
    for ext in extensions:
        pattern = re.escape(ext)
        m = re.search(rf'href="([^"]*{pattern}[^"]*)"', html, re.I)
        if m:
            return _abs_url(m.group(1), page_url)
        m = re.search(rf'(https?://[^\s"\'<>]+{pattern})', html, re.I)
        if m:
            return m.group(1)
    return None


def _text(node: Tag | None) -> str | None:
    if not node:
        return None
    text = node.get_text(" ", strip=True)
    return text or None


def _meta(soup: BeautifulSoup, prop: str) -> str | None:
    tag = soup.select_one(f'meta[property="{prop}"], meta[name="{prop}"]')
    return tag["content"].strip() if tag and tag.get("content") else None


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


def _table_value(soup: BeautifulSoup, label: str) -> str | None:
    for row in soup.select("table tr"):
        cells = row.find_all(["th", "td"])
        if len(cells) >= 2 and cells[0].get_text(strip=True).lower() == label:
            return cells[1].get_text(" ", strip=True)
    return None


def _label_value(soup: BeautifulSoup, label: str) -> str | None:
    val = _table_value(soup, label)
    if val:
        return val
    for node in soup.find_all(string=re.compile(label, re.I)):
        parent = node.parent
        if parent:
            sib = parent.find_next_sibling()
            if sib:
                text = sib.get_text(" ", strip=True)
                if text:
                    return text
    return None


def _genre_links(soup: BeautifulSoup) -> list[str]:
    genres: list[str] = []
    for tag in soup.select("a[href*='/genre/'], a[href*='/genres/']"):
        text = tag.get_text(strip=True)
        if text and text not in genres:
            genres.append(text)
    return genres


def _rating(soup: BeautifulSoup) -> tuple[float | None, int | None]:
    text = soup.get_text(" ", strip=True)
    m = re.search(r"(\d+(?:\.\d+)?)\s*\(\s*(\d+)\s*reviews?\s*\)", text, re.I)
    if m:
        return float(m.group(1)), int(m.group(2))
    m = re.search(r"(\d+(?:\.\d+)?)\s*/\s*5", text)
    if m:
        return float(m.group(1)), None
    return None, None


def _stats(soup: BeautifulSoup) -> tuple[int | None, str | None]:
    text = soup.get_text(" ", strip=True)
    downloads = None
    dm = re.search(r"([\d,]+)\s+downloads?", text, re.I)
    if dm:
        downloads = int(dm.group(1).replace(",", ""))
    sm = re.search(r"([\d.]+\s*(?:GB|MB|KB|TB))", text, re.I)
    size_hint = sm.group(1) if sm else None
    return downloads, size_hint


def _screenshots(soup: BeautifulSoup, page_url: str) -> list[str]:
    urls: list[str] = []
    for img in soup.select("figure img, .gallery img, .screenshot img, .swiper img"):
        src = img.get("src") or img.get("data-src")
        if src:
            full = urljoin(page_url, src)
            if full not in urls:
                urls.append(full)
    return urls[:6]


def _first_image(soup: BeautifulSoup, page_url: str) -> str | None:
    for img in soup.select("img[src]"):
        src = img.get("src", "")
        if src and "logo" not in src.lower():
            return urljoin(page_url, src)
    return None


def _hero_image(soup: BeautifulSoup, page_url: str) -> str | None:
    for sel in (".game-cover img", ".cover img", ".hero img", "img[src*='image/']"):
        img = soup.select_one(sel)
        if img and img.get("src"):
            return urljoin(page_url, img["src"])
    return _first_image(soup, page_url)


def _first_href(soup: BeautifulSoup, needle: str, page_url: str) -> str | None:
    for a in soup.select("a[href]"):
        href = a.get("href", "")
        if needle in href:
            return urljoin(page_url, href)
    return None


def _pkg_links(soup: BeautifulSoup, page_url: str) -> list[str]:
    urls: list[str] = []
    for a in soup.select("a[href]"):
        href = a.get("href", "")
        if ".pkg" in href.lower() or "/download" in href.lower():
            full = urljoin(page_url, href)
            if full not in urls:
                urls.append(full)
    return urls


def _abs_url(url: str, page_url: str) -> str:
    if url.startswith("//"):
        return "https:" + url
    if url.startswith(("http://", "https://")):
        return url
    return urljoin(page_url, url)

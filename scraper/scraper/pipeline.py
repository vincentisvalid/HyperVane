from __future__ import annotations

import tempfile
from dataclasses import dataclass, field
from pathlib import Path

from rich.console import Console
from rich.table import Table

from . import organizer
from .matchers import hash as hasher
from .platforms import get as get_platform
from .sources.base import ROMResult
from .sources.gameart import GameArtSource
from .sources.page_parser import PageMetadata
from .sources.romsfun import RomsFunSource

console = Console()


@dataclass
class ScrapeOptions:
    platform: str
    title: str | None = None
    amount: int | None = None
    art_only: bool = False
    roms_only: bool = False
    dry_run: bool = False
    output_dir: Path = Path("./output")
    verify: bool = True
    skip_existing: bool = True
    headless: bool = True
    request_delay_ms: int = 1500
    max_retries: int = 3
    user_data_dir: Path = Path("~/.cache/hypervane/browser")
    screenscraper_user: str = ""
    screenscraper_pass: str = ""
    thegamesdb_key: str = ""
    preferred_region: str = "us"
    max_screenshots: int = 3


@dataclass
class ScrapeResult:
    title: str
    platform: str
    description: str | None = None
    rom_paths: list[Path] = field(default_factory=list)
    art_paths: dict[str, str] = field(default_factory=dict)
    hashes: dict[str, dict[str, str]] = field(default_factory=dict)
    skipped: bool = False
    error: str | None = None


async def run(opts: ScrapeOptions) -> list[ScrapeResult]:
    pinfo = get_platform(opts.platform)
    results: list[ScrapeResult] = []

    async with RomsFunSource(
        user_data_dir=opts.user_data_dir,
        headless=opts.headless,
        request_delay_ms=opts.request_delay_ms,
        max_retries=opts.max_retries,
    ) as rom_source, GameArtSource(
        screenscraper_user=opts.screenscraper_user,
        screenscraper_pass=opts.screenscraper_pass,
        thegamesdb_key=opts.thegamesdb_key,
        preferred_region=opts.preferred_region,
        max_screenshots=opts.max_screenshots,
    ) as art_source:

        if opts.title:
            matches = await rom_source.search(opts.title, opts.platform)
            if not matches:
                console.print(f"[red]No matches for '{opts.title}' on {pinfo.name}[/]")
                return []
            titles_to_process = [(matches[0], None)]
            console.print(
                f"[green]Best match:[/] {matches[0].title} "
                f"(score={matches[0].score:.2f})"
            )
        else:
            console.print(
                f"[bold]Fetching platform listing:[/] {pinfo.name} "
                f"(amount={opts.amount or 'all'})"
            )
            detail_urls = await rom_source.list_platform(opts.platform, opts.amount)
            console.print(f"Found {len(detail_urls)} titles")
            titles_to_process = [
                (
                    ROMResult(
                        title=Path(url).stem.replace("-", " ").title(),
                        platform=opts.platform,
                        detail_url=url,
                        formats=pinfo.formats,
                        score=1.0,
                    ),
                    None,
                )
                for url in detail_urls
            ]

        for idx, (rom_result, _) in enumerate(titles_to_process, 1):
            console.rule(f"[{idx}/{len(titles_to_process)}] {rom_result.title}")
            result = await _process_one(
                rom_result=rom_result,
                opts=opts,
                pinfo=pinfo,
                rom_source=rom_source,
                art_source=art_source,
            )
            results.append(result)

    _print_summary(results)
    return results


async def _process_one(
    rom_result: ROMResult,
    opts: ScrapeOptions,
    pinfo,
    rom_source: RomsFunSource,
    art_source: GameArtSource,
) -> ScrapeResult:
    try:
        metadata = await rom_source.fetch_metadata(rom_result.detail_url)
    except Exception as exc:
        console.print(f"  [red]Metadata scrape failed:[/] {exc}")
        metadata = PageMetadata(title=rom_result.title)

    resolved_title = metadata.title or rom_result.title
    title_path = organizer.title_dir(opts.output_dir, pinfo.long_name, resolved_title)

    if opts.skip_existing and (title_path / "metadata.json").exists():
        console.print("  [dim]Skipping (already downloaded)[/]")
        return ScrapeResult(title=resolved_title, platform=opts.platform, skipped=True)

    if opts.dry_run:
        console.print(f"  [dim][DRY RUN] Would download: {resolved_title}[/]")
        if metadata.description:
            console.print(f"  [dim]{metadata.description[:120]}…[/]")
        return ScrapeResult(
            title=resolved_title,
            platform=opts.platform,
            description=metadata.description,
        )

    sr = ScrapeResult(
        title=resolved_title,
        platform=opts.platform,
        description=metadata.description,
    )

    if not opts.roms_only:
        try:
            art_dest = organizer.art_dir(title_path)
            romsfun_art = await rom_source.download_art(
                metadata, art_dest, max_screenshots=opts.max_screenshots
            )
            for key, name in romsfun_art.items():
                sr.art_paths[key] = f"art/{name}"

            if opts.screenscraper_user or opts.thegamesdb_key:
                extra = await art_source.fetch_art(
                    resolved_title, opts.platform, art_dest
                )
                for key in ("box_front", "box_back", "disc", "banner", "title_screen"):
                    val = getattr(extra, key)
                    if val and key not in sr.art_paths:
                        rel = Path(val).relative_to(title_path)
                        sr.art_paths[key] = str(rel)
                for i, ss in enumerate(extra.screenshots):
                    rel_key = f"screenshot_{i + 1:02d}"
                    if rel_key not in sr.art_paths:
                        rel = Path(ss).relative_to(title_path)
                        sr.art_paths[rel_key] = str(rel)
        except Exception as exc:
            console.print(f"  [yellow]Art fetch failed:[/] {exc}")

    if not opts.art_only:
        try:
            with tempfile.TemporaryDirectory(prefix="hypervane-") as tmp:
                rom_path = await rom_source.fetch(rom_result, Path(tmp))
                final = organizer.place_rom(rom_path, title_path)
                sr.rom_paths.append(final)

                if opts.verify:
                    hashes = hasher.compute_all(final)
                    sr.hashes[final.name] = hashes
                    console.print(f"  SHA1: {hashes['sha1']}")
        except Exception as exc:
            console.print(f"  [red]ROM download failed:[/] {exc}")
            sr.error = str(exc)

    rom_files_meta = [
        {
            "path": p.name,
            "format": p.suffix.lstrip("."),
            "size_bytes": p.stat().st_size if p.exists() else None,
            **sr.hashes.get(p.name, {}),
        }
        for p in sr.rom_paths
    ]

    organizer.write_metadata(
        title_path,
        title=resolved_title,
        platform=opts.platform,
        region=metadata.region or opts.preferred_region,
        description=metadata.description,
        developer=metadata.developer,
        publisher=metadata.publisher,
        release_date=metadata.release_date,
        genre=metadata.genres,
        rom_files=rom_files_meta,
        art=sr.art_paths,
        extra={
            "source_url": rom_result.detail_url,
            "rating": metadata.rating,
            "review_count": metadata.review_count,
            "download_count": metadata.download_count,
            "size_hint": metadata.size_hint,
        },
    )

    if metadata.description:
        console.print(f"  [dim]{metadata.description[:100]}…[/]")

    return sr


def _print_summary(results: list[ScrapeResult]) -> None:
    table = Table(title="Scrape Summary", show_lines=True)
    table.add_column("Title", style="bold")
    table.add_column("ROMs", justify="center")
    table.add_column("Art", justify="center")
    table.add_column("Status")

    for r in results:
        status = (
            "[dim]skipped[/]" if r.skipped
            else f"[red]{r.error}[/]" if r.error
            else "[green]ok[/]"
        )
        table.add_row(
            r.title,
            str(len(r.rom_paths)),
            str(len(r.art_paths)),
            status,
        )

    console.print(table)

from __future__ import annotations

import tempfile
from dataclasses import dataclass, field
from pathlib import Path

from rich.console import Console
from rich.table import Table

from . import PSPKG_PLATFORMS, ROMSFUN_PLATFORMS
from .browser import BrowserSession
from .catalog import Catalog, CatalogEntry
from .config import platform_label
from .organizer import art_dir, compute_hashes, place_rom, title_dir, write_metadata
from .sources.base import GameResult
from .sources.pspkg import PspkgSource
from .sources.romsfun import RomsFunSource

console = Console()


@dataclass
class DownloadOptions:
    platform: str
    title: str | None = None
    amount: int | None = None
    output_dir: Path = Path("./output")
    art_only: bool = False
    roms_only: bool = False
    dry_run: bool = False
    verify: bool = True
    skip_existing: bool = True
    headless: bool = True
    request_delay_ms: int = 1500
    max_retries: int = 3
    user_data_dir: Path = Path("~/.cache/hypervane/browser")
    max_screenshots: int = 3
    use_catalog: bool = True
    catalog: Catalog | None = None


@dataclass
class DownloadResult:
    title: str
    platform: str
    source: str
    rom_paths: list[Path] = field(default_factory=list)
    art_paths: dict[str, str] = field(default_factory=dict)
    skipped: bool = False
    error: str | None = None


def source_for_platform(platform: str, browser: BrowserSession, opts: DownloadOptions):
    if platform in ROMSFUN_PLATFORMS:
        return RomsFunSource(browser, opts.request_delay_ms, opts.max_retries)
    if platform in PSPKG_PLATFORMS:
        return PspkgSource(browser, opts.request_delay_ms, opts.max_retries)
    raise ValueError(
        f"Unsupported platform '{platform}'. "
        f"romsfun: {sorted(ROMSFUN_PLATFORMS)} | pspkg: {sorted(PSPKG_PLATFORMS)}"
    )


async def run_download(opts: DownloadOptions) -> list[DownloadResult]:
    platform = opts.platform.lower()
    results: list[DownloadResult] = []

    async with BrowserSession(
        user_data_dir=opts.user_data_dir,
        headless=opts.headless,
        request_delay_ms=opts.request_delay_ms,
    ) as browser:
        source = source_for_platform(platform, browser, opts)
        targets = await _resolve_targets(opts, source)

        if not targets:
            console.print("[yellow]No games matched the request[/]")
            return []

        for idx, target in enumerate(targets, 1):
            console.rule(f"[{idx}/{len(targets)}] {target.title}")
            result = await _process_one(target, source, opts)
            results.append(result)

    _print_summary(results)
    return results


async def _resolve_targets(opts: DownloadOptions, source) -> list[GameResult]:
    platform = opts.platform.lower()

    if opts.title:
        if opts.catalog and opts.use_catalog:
            catalog_hits = opts.catalog.search(opts.title, platform=platform, limit=5)
            if catalog_hits:
                console.print(
                    f"[dim]Catalog match:[/] {catalog_hits[0].title} "
                    f"(id={catalog_hits[0].game_id})"
                )
        online = await source.search(opts.title, platform)
        if online:
            best = online[0]
            console.print(
                f"[green]Online match ({best.source}):[/] {best.title} "
                f"(score={best.score:.2f})"
            )
            return [best]
        if opts.catalog and opts.use_catalog:
            catalog_hits = opts.catalog.search(opts.title, platform=platform, limit=1)
            if catalog_hits:
                entry = catalog_hits[0]
                return [
                    GameResult(
                        title=entry.title,
                        platform=platform,
                        detail_url="",
                        source=source.source_name,
                        score=entry.score,
                        catalog_id=entry.game_id,
                    )
                ]
        return []

    if opts.amount and opts.catalog and opts.use_catalog:
        entries = opts.catalog.list_platform(platform, opts.amount)
        if entries:
            console.print(f"[dim]Using {len(entries)} titles from catalog[/]")
            resolved: list[GameResult] = []
            for entry in entries:
                hits = await source.search(entry.title, platform)
                if hits:
                    hit = hits[0]
                    hit.catalog_id = entry.game_id
                    resolved.append(hit)
                else:
                    resolved.append(
                        GameResult(
                            title=entry.title,
                            platform=platform,
                            detail_url="",
                            source=source.source_name,
                            catalog_id=entry.game_id,
                        )
                    )
            return [r for r in resolved if r.detail_url][: opts.amount]

    return await source.list_platform(platform, opts.amount)


async def _process_one(target: GameResult, source, opts: DownloadOptions) -> DownloadResult:
    if not target.detail_url:
        return DownloadResult(
            title=target.title,
            platform=target.platform,
            source=source.source_name,
            error="No online detail URL found",
        )

    out_path = title_dir(opts.output_dir, target.platform, target.title)
    if opts.skip_existing and (out_path / "metadata.json").exists():
        console.print("  [dim]Skipping (already downloaded)[/]")
        return DownloadResult(
            title=target.title,
            platform=target.platform,
            source=source.source_name,
            skipped=True,
        )

    if opts.dry_run:
        meta = await source.fetch_metadata(target)
        console.print(f"  [dim][DRY RUN][/] {meta.title} via {source.source_name}")
        if meta.description:
            console.print(f"  [dim]{meta.description[:120]}…[/]")
        return DownloadResult(
            title=meta.title,
            platform=target.platform,
            source=source.source_name,
        )

    dr = DownloadResult(title=target.title, platform=target.platform, source=source.source_name)

    try:
        metadata = await source.fetch_metadata(target)
        dr.title = metadata.title
    except Exception as exc:
        console.print(f"  [yellow]Metadata failed:[/] {exc}")
        metadata = None

    if not opts.roms_only and metadata:
        try:
            saved = await source.download_art(
                metadata, art_dir(out_path), opts.max_screenshots
            )
            dr.art_paths.update(saved)
        except Exception as exc:
            console.print(f"  [yellow]Art failed:[/] {exc}")

    if not opts.art_only:
        try:
            with tempfile.TemporaryDirectory(prefix="romulus-") as tmp:
                rom_path = await source.download_rom(target, Path(tmp))
                final = place_rom(rom_path, out_path)
                dr.rom_paths.append(final)
                if opts.verify:
                    hashes = compute_hashes(final)
                    console.print(f"  SHA1: {hashes['sha1']}")
        except Exception as exc:
            console.print(f"  [red]ROM failed:[/] {exc}")
            dr.error = str(exc)

    rom_files = []
    for p in dr.rom_paths:
        entry = {"path": p.name, "format": p.suffix.lstrip("."), "size_bytes": p.stat().st_size}
        if opts.verify:
            entry.update(compute_hashes(p))
        rom_files.append(entry)

    payload = {
        "title": dr.title,
        "platform": target.platform,
        "platform_label": platform_label(target.platform),
        "catalog_id": target.catalog_id,
        "source": source.source_name,
        "source_url": target.detail_url,
        "description": metadata.description if metadata else None,
        "developer": metadata.developer if metadata else None,
        "publisher": metadata.publisher if metadata else None,
        "release_date": metadata.release_date if metadata else None,
        "genre": metadata.genres if metadata else [],
        "rating": metadata.rating if metadata else None,
        "download_count": metadata.download_count if metadata else None,
        "size_hint": metadata.size_hint if metadata else None,
        "rom_files": rom_files,
        "art": dr.art_paths,
    }
    write_metadata(out_path, payload)
    return dr


def _print_summary(results: list[DownloadResult]) -> None:
    table = Table(title="ROMULUS Download Summary", show_lines=True)
    table.add_column("Title", style="bold")
    table.add_column("Source")
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
            r.source,
            str(len(r.rom_paths)),
            str(len(r.art_paths)),
            status,
        )
    console.print(table)

from __future__ import annotations

import argparse
import asyncio
import sys
from pathlib import Path

import yaml
from rich.console import Console
from rich.table import Table

from . import ALL_PLATFORMS, PSPKG_PLATFORMS, ROMSFUN_PLATFORMS
from .catalog import Catalog
from .config import config_paths, load_config, platform_label
from .downloader import DownloadOptions, run_download

console = Console()
ROOT = Path(__file__).resolve().parent.parent


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="romulus",
        description="ROMULUS — The PlayStation ROM Downloader and Search Engine",
    )
    parser.add_argument("--config", type=Path, default=ROOT / "config.yaml")

    sub = parser.add_subparsers(dest="command", required=True)

    sp = sub.add_parser("search", help="Search the 24k-game PlayStation catalog")
    sp.add_argument("query", help="Title search query")
    sp.add_argument("--platform", "-p", choices=sorted(ALL_PLATFORMS))
    sp.add_argument("--limit", "-n", type=int, default=20)

    bp = sub.add_parser("catalog", help="Manage the local title catalog")
    bp_sub = bp.add_subparsers(dest="catalog_cmd", required=True)
    build = bp_sub.add_parser("build", help="Import gamelist CSVs into SQLite")
    build.add_argument("--clear", action="store_true")
    stats = bp_sub.add_parser("stats", help="Show catalog statistics")

    dp = sub.add_parser("download", help="Download ROMs, art, and metadata")
    dp.add_argument("--platform", "-p", required=True, choices=sorted(ALL_PLATFORMS))
    dp.add_argument("--title", "-t", help="Search and download a specific game")
    dp.add_argument(
        "--amount", "-n",
        type=int,
        help="Number of games to download from catalog/listing (ignored with --title)",
    )
    dp.add_argument("--output", "-o", type=Path)
    dp.add_argument("--art-only", action="store_true")
    dp.add_argument("--roms-only", action="store_true")
    dp.add_argument("--dry-run", action="store_true")
    dp.add_argument("--no-headless", dest="headless", action="store_false", default=True)
    dp.add_argument("--no-verify", dest="verify", action="store_false", default=True)
    dp.add_argument("--no-skip", dest="skip_existing", action="store_false", default=True)
    dp.add_argument("--no-catalog", dest="use_catalog", action="store_false", default=True)

    sub.add_parser("platforms", help="List supported platforms and sources")
    return parser


def cmd_platforms() -> None:
    table = Table(title="ROMULUS Platforms")
    table.add_column("ID", style="cyan")
    table.add_column("Name")
    table.add_column("Source")
    table.add_column("Formats")

    rows = [
        ("ps1", "PlayStation", "romsfun.com", "ISO, BIN/CUE"),
        ("ps2", "PlayStation 2", "romsfun.com", "ISO"),
        ("ps3", "PlayStation 3", "romsfun.com", "ISO, PKG"),
        ("psp", "PSP", "romsfun.com", "ISO"),
        ("ps4", "PlayStation 4", "pspkg.com", "PKG"),
        ("ps5", "PlayStation 5", "pspkg.com", "PKG"),
    ]
    for pid, name, src, fmt in rows:
        table.add_row(pid, name, src, fmt)
    console.print(table)
    console.print(
        "\n[dim]24,373 PlayStation titles indexed across 6 generations.[/]"
    )


def cmd_catalog_build(cfg: dict, paths: dict, clear: bool) -> None:
    catalog = Catalog(paths["catalog_db"])
    if clear:
        catalog.conn.execute("DELETE FROM games")
        catalog.conn.execute("DELETE FROM games_fts")
        catalog.conn.commit()
        console.print("[yellow]Cleared catalog[/]")

    total = catalog.import_gamelists(paths["gamelists_dir"])
    console.print(f"[green]Imported {total} rows[/] → {paths['catalog_db']}")
    console.print(f"Total indexed: [bold]{catalog.count()}[/] games")
    catalog.close()


def cmd_catalog_stats(paths: dict) -> None:
    catalog = Catalog(paths["catalog_db"])
    table = Table(title="ROMULUS Catalog")
    table.add_column("Platform")
    table.add_column("Games", justify="right")
    table.add_column("Source")

    total = 0
    for platform in sorted(ALL_PLATFORMS):
        count = catalog.count(platform)
        source = "romsfun.com" if platform in ROMSFUN_PLATFORMS else "pspkg.com"
        table.add_row(platform_label(platform), str(count), source)
        total += count
    table.add_row("[bold]Total[/]", f"[bold]{total}[/]", "")
    console.print(table)
    catalog.close()


def cmd_search(paths: dict, query: str, platform: str | None, limit: int) -> None:
    if not paths["catalog_db"].exists():
        console.print(
            "[red]Catalog not built.[/] Run: [bold]romulus catalog build[/]"
        )
        sys.exit(1)

    catalog = Catalog(paths["catalog_db"])
    hits = catalog.search(query, platform=platform, limit=limit)

    if not hits:
        console.print(f"[yellow]No matches for '{query}'[/]")
        catalog.close()
        return

    table = Table(title=f"Search: {query}")
    table.add_column("Score", justify="right")
    table.add_column("Platform")
    table.add_column("Title")
    table.add_column("ID")

    for hit in hits:
        table.add_row(
            f"{hit.score:.2f}",
            hit.platform.upper(),
            hit.title,
            hit.game_id,
        )
    console.print(table)
    catalog.close()


async def cmd_download(args: argparse.Namespace, cfg: dict, paths: dict) -> None:
    if not args.title and args.amount is None:
        args.amount = cfg.get("amount", 10)

    browser_cfg = cfg.get("browser", {})
    dl_cfg = cfg.get("download", {})

    catalog = None
    if paths["catalog_db"].exists() and args.use_catalog:
        catalog = Catalog(paths["catalog_db"])

    opts = DownloadOptions(
        platform=args.platform,
        title=args.title,
        amount=args.amount,
        output_dir=args.output or paths["output_dir"],
        art_only=args.art_only,
        roms_only=args.roms_only,
        dry_run=args.dry_run,
        verify=args.verify and dl_cfg.get("verify_hashes", True),
        skip_existing=args.skip_existing and dl_cfg.get("skip_existing", True),
        headless=args.headless and browser_cfg.get("headless", True),
        request_delay_ms=browser_cfg.get("request_delay_ms", 1500),
        max_retries=browser_cfg.get("max_retries", 3),
        user_data_dir=paths["user_data_dir"],
        max_screenshots=dl_cfg.get("max_screenshots", 3),
        use_catalog=args.use_catalog,
        catalog=catalog,
    )

    source = (
        "romsfun.com" if args.platform in ROMSFUN_PLATFORMS else "pspkg.com"
    )
    console.print(
        f"[bold cyan]ROMULUS[/] — {platform_label(args.platform)} via [magenta]{source}[/]"
        + (f", title=[cyan]{args.title}[/]" if args.title else "")
        + (
            f", amount=[cyan]{opts.amount}[/]"
            if opts.amount and not args.title
            else ""
        )
        + (" [yellow](dry-run)[/]" if opts.dry_run else "")
    )

    await run_download(opts)
    if catalog:
        catalog.close()


def main(argv: list[str] | None = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)
    cfg = load_config(args.config)
    paths = config_paths(cfg, ROOT)

    if args.command == "platforms":
        cmd_platforms()
    elif args.command == "catalog":
        if args.catalog_cmd == "build":
            cmd_catalog_build(cfg, paths, args.clear)
        elif args.catalog_cmd == "stats":
            cmd_catalog_stats(paths)
    elif args.command == "search":
        cmd_search(paths, args.query, args.platform, args.limit)
    elif args.command == "download":
        asyncio.run(cmd_download(args, cfg, paths))
    else:
        parser.print_help()


if __name__ == "__main__":
    main()

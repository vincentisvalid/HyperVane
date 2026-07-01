from __future__ import annotations

import argparse
import asyncio
import sys
from pathlib import Path

import yaml
from rich.console import Console
from rich.table import Table

from . import platforms as platform_registry
from .pipeline import ScrapeOptions, run

console = Console()

VALID_PLATFORMS = list(platform_registry.PLATFORMS)


def _load_config(path: Path) -> dict:
    if not path.exists():
        return {}
    with path.open() as f:
        return yaml.safe_load(f) or {}


def _resolve_path(val: str) -> Path:
    return Path(val).expanduser()


# --------------------------------------------------------------------------- CLI

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m scraper.main",
        description="HyperVane romsfun scraper — ROMs, cover art, titles, descriptions",
    )
    parser.add_argument("--config", type=Path, default=Path("config.yaml"), metavar="FILE")

    sub = parser.add_subparsers(dest="command", required=True)

    # ---- scrape ----
    sp = sub.add_parser("scrape", help="Scrape ROMs and/or art")

    sp.add_argument(
        "--platform", "-p",
        required=True,
        choices=VALID_PLATFORMS,
        metavar="PLATFORM",
        help=f"Target platform. One of: {', '.join(VALID_PLATFORMS)}",
    )
    sp.add_argument(
        "--title", "-t",
        default=None,
        help="Specific game title to scrape (searches instead of listing)",
    )
    sp.add_argument(
        "--amount", "-n",
        type=int,
        default=None,
        metavar="N",
        dest="amount",
        help=(
            "Number of ROMs to download from the platform listing. "
            "Overrides config 'amount'. Ignored when --title is given."
        ),
    )
    sp.add_argument("--output", "-o", type=Path, default=None, metavar="DIR")
    sp.add_argument("--art-only", action="store_true", help="Fetch art only, skip ROM download")
    sp.add_argument("--roms-only", action="store_true", help="Download ROMs only, skip art")
    sp.add_argument("--dry-run", action="store_true", help="Print what would be done without downloading")
    sp.add_argument("--no-headless", dest="headless", action="store_false", default=True,
                    help="Show browser window (useful for debugging Cloudflare challenges)")
    sp.add_argument("--no-verify", dest="verify", action="store_false", default=True,
                    help="Skip hash verification after download")
    sp.add_argument("--no-skip", dest="skip_existing", action="store_false", default=True,
                    help="Re-download even if title folder already exists")

    # ---- verify ----
    vp = sub.add_parser("verify", help="Verify ROM hashes in an output directory")
    vp.add_argument("--platform", "-p", required=True, choices=VALID_PLATFORMS)
    vp.add_argument("--dir", type=Path, required=True, metavar="DIR")

    # ---- platforms ----
    sub.add_parser("platforms", help="List supported platforms")

    return parser


# --------------------------------------------------------------------- commands

def cmd_platforms() -> None:
    table = Table(title="Supported Platforms")
    table.add_column("ID", style="bold cyan")
    table.add_column("Name")
    table.add_column("Formats")
    table.add_column("romsfun slug")

    for pinfo in platform_registry.PLATFORMS.values():
        table.add_row(
            pinfo.id,
            pinfo.long_name,
            ", ".join(pinfo.formats),
            pinfo.romsfun_slug,
        )

    console.print(table)


async def cmd_scrape(args: argparse.Namespace, cfg: dict) -> None:
    rom_cfg = cfg.get("roms", {})
    art_cfg = cfg.get("art", {})
    ss_cfg = cfg.get("sources", {}).get("screenscraper", {})
    tgdb_cfg = cfg.get("sources", {}).get("thegamesdb", {})
    rf_cfg = cfg.get("sources", {}).get("romsfun", {})

    # amount: CLI flag > config field > None (all)
    amount = args.amount
    if amount is None:
        amount = cfg.get("amount") or None

    opts = ScrapeOptions(
        platform=args.platform,
        title=args.title,
        amount=amount,
        art_only=args.art_only,
        roms_only=args.roms_only,
        dry_run=args.dry_run,
        output_dir=args.output or _resolve_path(cfg.get("output_dir", "./output")),
        verify=args.verify and rom_cfg.get("verify", True),
        skip_existing=args.skip_existing and rom_cfg.get("skip_existing", True),
        headless=args.headless and rf_cfg.get("headless", True),
        request_delay_ms=rf_cfg.get("request_delay_ms", 1500),
        max_retries=rf_cfg.get("max_retries", 3),
        user_data_dir=_resolve_path(rf_cfg.get("user_data_dir", "~/.cache/hypervane/browser")),
        screenscraper_user=ss_cfg.get("user", ""),
        screenscraper_pass=ss_cfg.get("pass", ""),
        thegamesdb_key=tgdb_cfg.get("api_key", ""),
        preferred_region=art_cfg.get("preferred_region", "us"),
        max_screenshots=art_cfg.get("max_screenshots", 3),
    )

    console.print(
        f"[bold]HyperVane Scraper[/] — platform=[cyan]{opts.platform}[/]"
        + (f", title=[cyan]{opts.title}[/]" if opts.title else "")
        + (
            f", amount=[cyan]{opts.amount}[/]"
            if opts.amount and not opts.title
            else ""
        )
        + (" [yellow](dry-run)[/]" if opts.dry_run else "")
    )

    await run(opts)


async def cmd_verify(args: argparse.Namespace) -> None:
    from .matchers import hash as hasher
    import json

    root = args.dir
    pinfo = platform_registry.get(args.platform)
    failures = 0

    for meta_file in sorted(root.rglob("metadata.json")):
        try:
            meta = json.loads(meta_file.read_text())
        except Exception:
            continue

        for rom_entry in meta.get("rom_files", []):
            rom_path = meta_file.parent / rom_entry["path"]
            if not rom_path.exists():
                console.print(f"[red]MISSING[/] {rom_path}")
                failures += 1
                continue

            expected = {k: rom_entry[k] for k in ("crc32", "md5", "sha1", "sha256") if k in rom_entry}
            if not expected:
                console.print(f"[dim]NO HASH[/] {rom_path.name}")
                continue

            ok = hasher.verify(rom_path, expected)
            if ok:
                console.print(f"[green]OK[/]  {rom_path.name}")
            else:
                console.print(f"[red]FAIL[/] {rom_path.name}")
                failures += 1

    if failures:
        console.print(f"\n[red]{failures} verification failure(s)[/]")
        sys.exit(1)
    else:
        console.print("\n[green]All hashes verified[/]")


# -------------------------------------------------------------------------- main

def main(argv: list[str] | None = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)
    cfg = _load_config(args.config)

    if args.command == "platforms":
        cmd_platforms()
    elif args.command == "scrape":
        asyncio.run(cmd_scrape(args, cfg))
    elif args.command == "verify":
        asyncio.run(cmd_verify(args))
    else:
        parser.print_help()


if __name__ == "__main__":
    main()

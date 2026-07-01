# HyperVane ROM Scraper

A metadata and asset scraper for ROM libraries. Fetches game art (box art, disc art, screenshots, banners) and ROM files in supported formats, organizing output by platform and title.

---

## Supported Formats

| Format       | Extension(s)    | Platforms                        |
|--------------|-----------------|----------------------------------|
| ISO image    | `.iso`          | PS1, PS2, PS3, PSP, PS4, PS5, PC |
| BIN/CUE      | `.bin` + `.cue` | PS1, Saturn, PC-Engine, etc.     |
| PS3 Package  | `.pkg`          | PS3                              |
| PS4 Package  | `.pkg`          | PS4                              |
| PS5 Package  | `.pkg`          | PS5                              |

---

## Architecture

```
scraper/
├── SCRAPER.md              # this file
├── config.yaml             # runtime configuration
├── scraper/
│   ├── __init__.py
│   ├── main.py             # CLI entry point
│   ├── pipeline.py         # orchestrates fetch → match → download → organize
│   ├── sources/
│   │   ├── base.py         # abstract Source class
│   │   ├── gameart.py      # game art scraper (box, disc, screenshot, banner)
│   │   └── romsfun.py      # romsfun.com ROM scraper (Playwright-based)
│   ├── platforms/
│   │   ├── base.py         # abstract Platform class
│   │   ├── ps1.py          # BIN/CUE, ISO
│   │   ├── ps2.py          # ISO
│   │   ├── ps3.py          # ISO, PKG
│   │   ├── ps4.py          # PKG
│   │   └── ps5.py          # PKG
│   ├── matchers/
│   │   ├── title.py        # fuzzy title matching
│   │   └── hash.py         # CRC32/MD5/SHA1 ROM verification
│   └── organizer.py        # output directory layout
├── tests/
│   ├── test_matchers.py
│   ├── test_platforms.py
│   └── fixtures/
└── output/                 # default output root (configurable)
```

---

## Data Sources

### Game Art
- **ScreenScraper** (`screenscraper.fr`) — primary art source; supports box art, disc art, screenshots, fan art, video snaps
- **TheGamesDB** (`thegamesdb.net`) — fallback art source
- **IGDB** (`igdb.com`) — supplemental metadata and screenshots

Art types fetched per title (configurable):

| Key             | Description                        |
|-----------------|------------------------------------|
| `box_front`     | Front box art                      |
| `box_back`      | Back box art                       |
| `box_3d`        | 3D rendered box                    |
| `disc`          | Disc / media art                   |
| `screenshot`    | In-game screenshots (up to N)      |
| `banner`        | Wide banner / marquee              |
| `title_screen`  | Title screen capture               |
| `video`         | Gameplay video snap (`.mp4`)       |

### ROM Files — romsfun.com

ROM files are scraped from **romsfun.com** via `sources/romsfun.py`.

#### Why Playwright

romsfun.com is behind Cloudflare's bot protection. Plain `requests`/`httpx` calls return HTTP 403. The scraper drives a real Chromium browser via [Playwright](https://playwright.dev/python/) with the [`playwright-stealth`](https://pypi.org/project/playwright-stealth/) plugin to pass the JS challenge. A persistent browser context stores the solved Cloudflare cookie (`cf_clearance`) so it only needs to be solved once per session.

#### Platform URL slugs

| Platform | romsfun.com slug         | Listing URL                                        |
|----------|--------------------------|----------------------------------------------------|
| PS1      | `playstation`            | `https://romsfun.com/roms/playstation/`            |
| PS2      | `playstation-2`          | `https://romsfun.com/roms/playstation-2/`          |
| PS3      | `playstation-3`          | `https://romsfun.com/roms/playstation-3/`          |
| PS4      | `playstation-4`          | `https://romsfun.com/roms/playstation-4/`          |
| PS5      | `playstation-5`          | `https://romsfun.com/roms/playstation-5/`          |
| PSP      | `psp`                    | `https://romsfun.com/roms/psp/`                    |

#### Search

```
GET https://romsfun.com/search/?q=<url-encoded-title>
```

Results are parsed from the listing cards on the search results page. Each card contains:
- Title text
- Platform badge
- Link to the ROM detail page (`/roms/<platform-slug>/<title-slug>.html`)

#### Scraping flow

```
1. search(title, platform)
   └─ GET /search/?q=<title>
      └─ parse result cards → filter by platform slug
         └─ fuzzy-match card title against query → return list[ROMResult]

2. fetch(result, dest)
   └─ GET /roms/<platform>/<slug>.html         (detail page)
      └─ locate download section
         └─ click "Download" button / resolve redirect chain
            └─ intercept final file URL (CDN link)
               └─ stream download → dest / verify hash
```

#### Download link resolution

Download buttons on romsfun.com are JavaScript-driven and may redirect through one or more intermediary pages before reaching the CDN file URL. The Playwright context follows all redirects. Once the browser navigates to a URL whose path ends with a known ROM extension (`.iso`, `.pkg`, `.bin`, `.cue`), that URL is captured and handed off to a streaming `httpx` download (reusing the same cookies) so the file is written directly to disk without buffering in memory.

#### Session management

```python
# sources/romsfun.py (outline)
class RomsFunSource(Source):
    PLATFORM_SLUGS = {
        "ps1": "playstation",
        "ps2": "playstation-2",
        "ps3": "playstation-3",
        "ps4": "playstation-4",
        "ps5": "playstation-5",
        "psp": "psp",
    }
    BASE_URL = "https://romsfun.com"

    def __init__(self, user_data_dir: Path, headless: bool = True):
        # persistent context preserves cf_clearance cookie across runs
        self._user_data_dir = user_data_dir
        self._headless = headless

    async def search(self, title: str, platform: str) -> list[ROMResult]: ...
    async def fetch(self, result: ROMResult, dest: Path) -> Path: ...
```

The `user_data_dir` is written to `~/.cache/hypervane/browser/` by default so the solved Cloudflare cookie persists between scraper invocations.

#### Source interface

All sources implement the same async interface defined in `sources/base.py`:

```python
class Source(ABC):
    async def search(self, title: str, platform: str) -> list[ROMResult]: ...
    async def fetch(self, result: ROMResult, dest: Path) -> Path: ...

@dataclass
class ROMResult:
    title: str
    platform: str
    detail_url: str
    formats: list[str]   # ["iso"], ["pkg"], ["bin_cue"], etc.
    size_hint: str | None
    score: float          # fuzzy match confidence 0–1
```

---

## Configuration

`config.yaml`:

```yaml
output_dir: ./output

art:
  enabled: true
  types:
    - box_front
    - box_back
    - disc
    - screenshot
    - banner
  max_screenshots: 3
  preferred_region: us   # us | eu | jp
  resolution: 1x         # 1x | 2x | original

roms:
  enabled: true
  formats:
    ps1:  [bin_cue, iso]
    ps2:  [iso]
    ps3:  [iso, pkg]
    ps4:  [pkg]
    ps5:  [pkg]
  verify: true            # CRC/hash check after download
  skip_existing: true

sources:
  # Art sources
  screenscraper:
    user: ""
    pass: ""
  thegamesdb:
    api_key: ""
  igdb:
    client_id: ""
    client_secret: ""

  # ROM source
  romsfun:
    enabled: true
    headless: true                              # set false to watch the browser
    user_data_dir: ~/.cache/hypervane/browser   # persists cf_clearance cookie
    request_delay_ms: 1500                      # polite delay between requests
    max_retries: 3

platforms:
  - ps1
  - ps2
  - ps3
  - ps4
  - ps5
```

---

## Output Layout

```
output/
└── PlayStation 3/
    └── Metal Gear Solid 4/
        ├── Metal Gear Solid 4.iso
        ├── Metal Gear Solid 4.pkg
        ├── art/
        │   ├── box_front.png
        │   ├── box_back.png
        │   ├── disc.png
        │   ├── banner.png
        │   ├── screenshot_01.png
        │   ├── screenshot_02.png
        │   └── screenshot_03.png
        └── metadata.json
```

`metadata.json` schema:

```json
{
  "title": "Metal Gear Solid 4: Guns of the Patriots",
  "platform": "ps3",
  "region": "us",
  "release_date": "2008-06-12",
  "developer": "Konami",
  "publisher": "Konami",
  "genre": ["Action", "Stealth"],
  "players": 1,
  "rom_files": [
    {
      "path": "Metal Gear Solid 4.iso",
      "format": "iso",
      "size_bytes": 25769803776,
      "sha1": "...",
      "md5": "...",
      "crc32": "..."
    }
  ],
  "art": {
    "box_front": "art/box_front.png",
    "disc": "art/disc.png"
  },
  "scraped_at": "2026-06-30T00:00:00Z"
}
```

---

## CLI Usage

```bash
# Scrape art + ROMs for a single title
python -m scraper.main scrape --title "Metal Gear Solid 4" --platform ps3

# Scrape an entire platform
python -m scraper.main scrape --platform ps3

# Art only (no ROM download)
python -m scraper.main scrape --platform ps4 --art-only

# ROM only (no art)
python -m scraper.main scrape --platform ps5 --roms-only

# Dry run — print matches without downloading
python -m scraper.main scrape --platform ps3 --dry-run

# Verify existing ROM hashes against known-good database
python -m scraper.main verify --platform ps3 --dir ./output/PlayStation\ 3/

# List supported platforms
python -m scraper.main platforms
```

---

## PKG File Handling (PS3 / PS4 / PS5)

`.pkg` files are Sony's package format. The scraper handles them as opaque blobs — it does not unpack or decrypt them. Download, hash verification, and filesystem placement are performed the same as for `.iso` files.

| Platform | PKG type      | Notes                                          |
|----------|---------------|------------------------------------------------|
| PS3      | NPDRM retail  | Requires valid license to run on hardware      |
| PS4      | `fpkg` / retail | Regional variants tracked separately          |
| PS5      | `fpkg` / retail | File sizes typically 20–100 GB                |

PKG integrity is verified via the embedded SHA-256 digest in the package header after download.

---

## BIN/CUE Handling (PS1 / Saturn)

Multi-track BIN/CUE sets are kept together as a unit. The scraper:

1. Downloads the `.cue` sheet first to determine track count and `.bin` filenames.
2. Downloads each `.bin` track in order.
3. Verifies the full set with CRC32 checksums from the DAT database.
4. Stores everything in a single title directory; the `.cue` file is the entry point.

---

## Hash Verification

The `matchers/hash.py` module compares downloaded files against No-Intro and Redump DAT databases:

- **No-Intro** — cartridge-based platforms
- **Redump** — disc-based platforms (PS1, PS2, PS3, PS4, PS5)

DAT files are fetched and cached locally on first run. Hash algorithms checked in order: CRC32 → MD5 → SHA1.

---

## Dependencies

```
playwright          # browser automation
playwright-stealth  # Cloudflare fingerprint bypass
httpx[http2]        # streaming file downloads (shares cookies with Playwright context)
beautifulsoup4      # HTML parsing for search results and detail pages
rapidfuzz           # fuzzy title matching
pyyaml              # config parsing
rich                # CLI progress bars and logging
```

Install browser binaries after `pip install playwright`:

```bash
playwright install chromium
```

---

## Legal Notice

This tool is intended for use with software you own. Downloading or distributing copyrighted game files without authorization from the rights holder may violate copyright law in your jurisdiction. The authors assume no liability for misuse.

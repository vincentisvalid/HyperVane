"""ROM ID generation and hashing utilities.

Generates standardized ROM identifiers in the format::

    SYSTEM-REGION-TITLE-XXXXX

e.g. ``PS2-SCUS-SLYCOOPER-00001``

The suffix (XXXXX) is derived from the SHA-1 hash of the file content
so that the same ROM always produces the same identifier.
"""

from __future__ import annotations

import hashlib
import logging
import re
import zlib
from pathlib import Path
from typing import Protocol

log = logging.getLogger(__name__)

# Chunk size for streaming hash computation (128 KiB).
_CHUNK_SIZE = 128 * 1024


# ── Public type ────────────────────────────────────────────────────────────────

class RomHashes(Protocol):
    """Expected shape of objects returned by :func:`compute_all`."""
    crc32: str
    md5: str
    sha1: str
    sha256: str
    file_size: int


# ── RomIDGenerator ────────────────────────────────────────────────────────────

class RomIDGenerator:
    """Generate standard ROM identifiers from file metadata + content hash."""

    # Serial number patterns (matched case-insensitively in the title/region path)
    _SERIAL_PATTERNS = re.compile(
        r"(?P<serial>[A-Z]{2,4}-?\d{3,5})",
        re.IGNORECASE,
    )

    # ── Public API ────────────────────────────────────────────────────────

    @classmethod
    def from_metadata(
        cls,
        platform: str,
        region: str,
        title: str,
        sha1_hash: str = "",
        counter: int = 0,
    ) -> str:
        """Build a ROM ID from individual metadata components.

        Parameters
        ----------
        platform : str
            Platform/system name (e.g. ``"PS2"``, ``"SNES"``, ``"GBA"``).
        region : str
            Region or serial prefix (e.g. ``"USA"``, ``"SCUS"``, ``"PAL"``).
        title : str
            Game title (spaces and special chars will be stripped).
        sha1_hash : str
            Full 40-character SHA-1 hex string.  Only the first 5 characters
            are used for the suffix.
        counter : int
            Legacy sequential counter (5-digit zero-padded).  When > 0 this
            is used instead of the hash suffix.

        Returns
        -------
        str
            e.g. ``"PS2-SCUS-SLYCOOPER-3A8F2"``
        """
        system = cls._normalise_system(platform)
        region_part = cls._normalise_region(region, title)
        title_slug = cls._normalise_title(title)

        if counter > 0:
            suffix = f"{counter:05d}"
        elif sha1_hash:
            suffix = sha1_hash[:5].upper()
        else:
            # Derive a deterministic suffix from the title alone
            suffix = hashlib.sha1(title.encode("utf-8")).hexdigest()[:5].upper()

        return f"{system}-{region_part}-{title_slug}-{suffix}"

    @classmethod
    def from_hashes(
        cls,
        platform: str,
        region: str,
        title: str,
        hashes: RomHashes,
    ) -> str:
        """Convenience: build ID from a :class:`RomHashes` object."""
        return cls.from_metadata(platform, region, title, hashes.sha1)

    # ── Internal normalisers ──────────────────────────────────────────────

    @staticmethod
    def _normalise_system(raw: str) -> str:
        s = raw.upper().strip()
        # Map common variants to canonical forms
        MAPPINGS = {
            "SUPER NINTENDO": "SNES",
            "NINTENDO ENTERTAINMENT SYSTEM": "NES",
            "GAME BOY ADVANCE": "GBA",
            "GAME BOY COLOR": "GBC",
            "GAME BOY": "GB",
            "NINTENDO 64": "N64",
            "NINTENDO DS": "NDS",
            "NINTENDO 3DS": "3DS",
            "PLAYSTATION": "PS1",
            "PLAYSTATION 2": "PS2",
            "PLAYSTATION 3": "PS3",
            "PLAYSTATION 4": "PS4",
            "PLAYSTATION PORTABLE": "PSP",
            "PLAYSTATION VITA": "PSV",
            "SEGA GENESIS": "GENESIS",
            "SEGA MEGA DRIVE": "GENESIS",
            "SEGA DREAMCAST": "DREAMCAST",
            "SEGA SATURN": "SATURN",
            "SEGA CD": "SEGACD",
            "SEGA 32X": "SEGA32X",
            "MICROSOFT XBOX": "XBOX",
            "XBOX 360": "XBOX360",
            "XBOX ONE": "XBOXONE",
            "NINTENDO GAMECUBE": "GC",
            "NINTENDO WII": "WII",
            "NINTENDO WII U": "WIIU",
            "NINTENDO SWITCH": "SWITCH",
        }
        return MAPPINGS.get(s, re.sub(r"[^A-Z0-9]", "", s))

    @staticmethod
    def _normalise_region(region: str, title: str) -> str:
        # Prefer a serial number prefix found in the title/region field
        m = RomIDGenerator._SERIAL_PATTERNS.search(region)
        if m:
            # Only keep the alphabetic prefix (e.g. "SCUS" from "SCUS-97101")
            serial = m.group("serial").upper()
            return re.sub(r"[^A-Z]", "", serial)[:4]
        m = RomIDGenerator._SERIAL_PATTERNS.search(title)
        if m:
            serial = m.group("serial").upper()
            return re.sub(r"[^A-Z]", "", serial)[:4]

        r = region.upper().strip()
        MAPPINGS = {
            "UNITED STATES": "USA",
            "US": "USA",
            "AMERICA": "USA",
            "NORTH AMERICA": "USA",
            "NTSC": "USA",
            "JAPAN": "JPN",
            "JAPANESE": "JPN",
            "JP": "JPN",
            "EUROPE": "EUR",
            "EUROPEAN": "EUR",
            "PAL": "EUR",
            "UK": "EUR",
            "WORLD": "WORLD",
            "ASIA": "ASIA",
            "KOREA": "KOR",
            "CHINA": "CHN",
            "BRAZIL": "BRA",
            "AUSTRALIA": "AUS",
        }
        return MAPPINGS.get(r, r[:4])

    @staticmethod
    def _normalise_title(raw: str) -> str:
        """Strip special characters, collapse whitespace, uppercase."""
        title = raw.upper()
        title = re.sub(r"[^A-Z0-9]", "", title)
        return title


# ── RomHasher ─────────────────────────────────────────────────────────────────

class RomHasher:
    """Compute a full set of hashes for a ROM file in a single pass."""

    @staticmethod
    def compute_all(path: str | Path) -> dict[str, str | int]:
        """Stream over the file once, computing CRC32, MD5, SHA-1, SHA-256.

        Returns (``crc32``, ``md5``, ``sha1``, ``sha256``, ``file_size``).
        """
        crc = 0
        md5 = hashlib.md5()
        sha1 = hashlib.sha1()
        sha256 = hashlib.sha256()
        size = 0

        with open(path, "rb") as f:
            while True:
                chunk = f.read(_CHUNK_SIZE)
                if not chunk:
                    break
                crc = zlib.crc32(chunk, crc)
                md5.update(chunk)
                sha1.update(chunk)
                sha256.update(chunk)
                size += len(chunk)

        return {
            "crc32": f"{crc & 0xFFFFFFFF:08x}",
            "md5": md5.hexdigest(),
            "sha1": sha1.hexdigest(),
            "sha256": sha256.hexdigest(),
            "file_size": size,
        }

    @staticmethod
    def compute_crc32(path: str | Path) -> str:
        return str(RomHasher.compute_all(path)["crc32"])

    @staticmethod
    def compute_sha1(path: str | Path) -> str:
        return str(RomHasher.compute_all(path)["sha1"])

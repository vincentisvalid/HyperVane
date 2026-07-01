from __future__ import annotations

import binascii
import hashlib
from pathlib import Path


_CHUNK = 1 << 20  # 1 MiB


def crc32(path: Path) -> str:
    val = 0
    with path.open("rb") as f:
        while chunk := f.read(_CHUNK):
            val = binascii.crc32(chunk, val)
    return format(val & 0xFFFFFFFF, "08x")


def md5(path: Path) -> str:
    h = hashlib.md5()
    with path.open("rb") as f:
        while chunk := f.read(_CHUNK):
            h.update(chunk)
    return h.hexdigest()


def sha1(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        while chunk := f.read(_CHUNK):
            h.update(chunk)
    return h.hexdigest()


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(_CHUNK):
            h.update(chunk)
    return h.hexdigest()


def compute_all(path: Path) -> dict[str, str]:
    crc = 0
    h_md5 = hashlib.md5()
    h_sha1 = hashlib.sha1()
    h_sha256 = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(_CHUNK):
            crc = binascii.crc32(chunk, crc)
            h_md5.update(chunk)
            h_sha1.update(chunk)
            h_sha256.update(chunk)
    return {
        "crc32": format(crc & 0xFFFFFFFF, "08x"),
        "md5": h_md5.hexdigest(),
        "sha1": h_sha1.hexdigest(),
        "sha256": h_sha256.hexdigest(),
    }


def verify(path: Path, expected: dict[str, str]) -> bool:
    """Check any of crc32/md5/sha1/sha256 provided in expected dict."""
    computed = compute_all(path)
    for alg, val in expected.items():
        if alg in computed:
            if computed[alg].lower() != val.lower():
                return False
    return True

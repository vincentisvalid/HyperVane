from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class ROMResult:
    title: str
    platform: str
    detail_url: str
    formats: list[str]
    size_hint: str | None = None
    score: float = 0.0


@dataclass
class ArtResult:
    title: str
    platform: str
    box_front: str | None = None
    box_back: str | None = None
    box_3d: str | None = None
    disc: str | None = None
    screenshots: list[str] = field(default_factory=list)
    banner: str | None = None
    title_screen: str | None = None
    video: str | None = None


class Source(ABC):
    @abstractmethod
    async def search(self, title: str, platform: str) -> list[ROMResult]: ...

    @abstractmethod
    async def fetch(self, result: ROMResult, dest: Path) -> Path: ...

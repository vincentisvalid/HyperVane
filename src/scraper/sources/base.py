from __future__ import annotations
import logging
from pathlib import Path
import httpx 
import json

log = logging.getLogger("hypervane.scraper.sources")

class BaseSource(ABC):
    def __init__(self, config: dict):
        self.config = config
        # Add common setup/initialization variables specific to the source type (e.g., API keys)
        pass

    @abstractmethod
    async def search_titles(self, title: str, platform: str) -> list[ROMResult]:
        """Performs fuzzy and exact matching against its remote index."""
        raise NotImplementedError
    
    @abstractmethod
    async def fetch_roms(self, result: ROMResult, dest: Path) -> None:
        """Downloads and verifies the specific ROM file."""
        pass

# Structure for common data types (ROMResult needs to be defined here or imported)
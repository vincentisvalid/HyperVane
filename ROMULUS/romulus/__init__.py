"""ROMULUS — The PlayStation ROM Downloader and Search Engine."""

__version__ = "0.1.0"

ROMSFUN_PLATFORMS = frozenset({"ps1", "ps2", "ps3", "psp"})
PSPKG_PLATFORMS = frozenset({"ps4", "ps5"})
ALL_PLATFORMS = ROMSFUN_PLATFORMS | PSPKG_PLATFORMS

PLATFORM_NAMES = {
    "ps1": "PlayStation",
    "ps2": "PlayStation 2",
    "ps3": "PlayStation 3",
    "ps4": "PlayStation 4",
    "ps5": "PlayStation 5",
    "psp": "PlayStation Portable",
}

GAMELIST_FILES = {
    "ps1": "gameslist (ps1).csv",
    "ps2": "gameslist (ps2).csv",
    "ps3": "gameslist (ps3).csv",
    "ps4": "gameslist (ps4).csv",
    "ps5": "gameslist[PS5].csv",
    "psp": "gameslist[PSP].csv",
}

from .base import PlatformInfo
from .ps1 import INFO as PS1
from .ps2 import INFO as PS2
from .ps3 import INFO as PS3
from .ps4 import INFO as PS4
from .psp import INFO as PSP

PLATFORMS: dict[str, PlatformInfo] = {
    p.id: p for p in [PS1, PS2, PS3, PS4, PSP]
}


def get(platform_id: str) -> PlatformInfo:
    try:
        return PLATFORMS[platform_id.lower()]
    except KeyError:
        raise ValueError(f"Unknown platform '{platform_id}'. Valid: {list(PLATFORMS)}")

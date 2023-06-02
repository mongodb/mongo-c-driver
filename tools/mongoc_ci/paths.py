from dagon import option
from pathlib import Path
import os
import sys

from .platform import OperatingSystem as OS


def _get_default_cache_dir() -> Path:
    opsys = OS.current()
    if opsys.is_windows:
        return Path(os.environ["LocalAppData"]) / "mongodb-ci"
    elif opsys.is_darwin:
        return Path("~/Library/Caches").expanduser() / "mongodb-ci"
    elif opsys.is_unix_like:
        base = os.environ.get("XDG_CACHE_HOME", "~/.cache/")
        return Path(base).expanduser() / "mongodb-ci"
    else:
        return Path("~/.cache").expanduser()


OPT_CACHES_DIR = option.add(
    "cache.dir",
    type=Path,
    doc="Directory in which to store caches",
    default_factory=_get_default_cache_dir,
)
"The caching directory provided by the “cache.dir” Dagon option"

EXE_SUFFIX = ".exe" if os.name == "nt" else ""
"The default suffix of executable files on this platform"

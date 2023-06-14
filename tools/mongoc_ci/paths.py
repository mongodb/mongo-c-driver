from dagon import option
from pathlib import Path
import os

from .platform import OperatingSystem as OS


def _get_default_cache_dir() -> Path:
    if OS.Current.is_windows:
        return Path(os.environ["LocalAppData"]) / "mongodb-ci"
    elif OS.Current.is_darwin:
        return Path("~/Library/Caches").expanduser() / "mongodb-ci"
    elif OS.Current.is_unix_like:
        base = os.environ.get("XDG_CACHE_HOME", "~/.cache/")
        return Path(base).expanduser() / "mongodb-ci"
    else:
        return Path("~/.cache").expanduser()


OPT_CACHE_DIR = option.add(
    "cache.dir",
    type=Path,
    doc="Directory in which to store caches",
    default_factory=_get_default_cache_dir,
)
"The caching directory provided by the “cache.dir” Dagon option"

EXE_SUFFIX = ".exe" if os.name == "nt" else ""
"The default suffix of executable files on this platform"

_THIS_FILE = Path(__file__).resolve()
MONGOC_DIR = _THIS_FILE.parent.parent.parent
"The root source directory of the mongo-c-driver repository"

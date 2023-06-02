from __future__ import annotations

import os
from pathlib import Path
import platform
import re
import sys
from typing import Literal

from dagon import option, http, fs, ar, ui, task, proc

from .paths import EXE_SUFFIX, OPT_CACHES_DIR
from .platform import OperatingSystem, Architecture


def ninja_progress(message: proc.ProcessOutputItem) -> None:
    line = message.out.decode()
    ui.status(line)
    mat = re.search(r"\b(\d+)/(\d+)\b", line)
    if not mat:
        return
    num, den = mat.groups()
    prog = int(num) / int(den)
    ui.progress(prog)


async def _build_from_source(version: str, cache_dir: Path) -> Path:
    ninja_exe = cache_dir / f"ninja{EXE_SUFFIX}"
    if ninja_exe.is_file():
        return ninja_exe
    cache_dir.mkdir(exist_ok=True, parents=True)
    # Begin async removal:
    build_dir = cache_dir / "build"
    removal = fs.remove(build_dir, absent_ok=True, recurse=True)
    task.cleanup(lambda: removal, when="now")
    build_dir.mkdir(parents=True)

    # Download the source archive:
    ui.status(f"Downloading Ninja {version}")
    url = f"https://github.com/ninja-build/ninja/archive/refs/tags/v{version}.tar.gz"
    src_tgz = cache_dir / "ninja-src.tgz"
    await http.download(url, destination=src_tgz, if_exists="keep")

    # Expand the source archive:
    src_dir = cache_dir / "source"
    ui.status("Expanding archive")
    await ar.expand(
        src_tgz,
        destination=src_dir,
        strip_components=1,
        on_extract="print-status",
        if_exists="replace",
    )

    # Build it
    await proc.run(
        [
            sys.executable,
            "-u",
            src_dir / "configure.py",
            "--bootstrap",
        ],
        cwd=build_dir,
        on_output=ninja_progress,
    )
    await fs.copy_file(build_dir / f"ninja{EXE_SUFFIX}", ninja_exe)
    return ninja_exe


async def _get_prebuilt(version: str, cache_dir: Path, plat: str) -> Path:
    url = f"https://github.com/ninja-build/ninja/releases/download/v{version}"
    url = f"{url}/ninja-{plat}.zip"
    ui.status(f"Downloading [{url}]")
    async with http.download_tmp(url, suffix="ninja.zip") as zf:
        await ar.expand(zf, destination=cache_dir, if_exists="merge")
    ui.print(f"Downloaded pre-built Ninja executable from [{url}]")
    return cache_dir / f"ninja{EXE_SUFFIX}"


async def auto_get_ninja(version: str, *, cache_root: Path | None = None) -> Path:
    """Obtain a cached version of Ninja for the current host and platform"""
    cache_root = cache_root or OPT_CACHES_DIR.get()
    ninja_cache_dir = cache_root / f"ninja-{version}"
    expect_exe = ninja_cache_dir / f"ninja{EXE_SUFFIX}"
    if expect_exe.is_file():
        return expect_exe

    osys = OperatingSystem.current()
    if osys.is_windows:
        get = _get_prebuilt(version, ninja_cache_dir, "win")
    elif osys.is_darwin:
        get = _get_prebuilt(version, ninja_cache_dir, "mac")
    elif osys.is_linux and Architecture.current() is Architecture.x86_64 and not Path("/etc/alpine-release").is_file():
        get = _get_prebuilt(version, ninja_cache_dir, "linux")
    else:
        ui.print(f"OS is {osys=}")
        get = _build_from_source(version, ninja_cache_dir)
    return await get

from __future__ import annotations
import os
from pathlib import Path
import re

from dagon import ui, http, ar

from .paths import OPT_CACHES_DIR, EXE_SUFFIX
from .platform import OperatingSystem, Architecture


class CMakeExecutable:
    def __init__(self, exe: Path) -> None:
        self.executable = exe


class Installation:
    def __init__(self, root: Path) -> None:
        self.root = root

    @property
    def cmake(self) -> CMakeExecutable:
        return CMakeExecutable(self.root / f"bin/cmake{EXE_SUFFIX}")


async def get_cmake_installation(version: str, *, cache_root: Path | None = None) -> Installation:
    cache_root = cache_root or OPT_CACHES_DIR.get()
    install_root = cache_root / f"cmake-{version}"
    cmake_exe = install_root / f"bin/cmake{EXE_SUFFIX}"
    if cmake_exe.is_file():
        return Installation(install_root)

    osys = OperatingSystem.current()
    if osys.is_windows or osys.is_darwin:
        return await _download_prebuilt(version, install_root)
    if osys.is_linux and Architecture.current() in (Architecture.x86_64, Architecture.ARM64):
        return await _download_prebuilt(version, install_root)
    return await _build_from_source(version, install_root)


async def _download_prebuilt(version: str, install_root: Path) -> Installation:
    osys = OperatingSystem.current()
    arch = Architecture.current()
    nstrip = 0
    if osys.is_windows:
        plat = "windows"
        ext = "zip"
        nstrip = 1
    elif osys.is_darwin:
        plat = "macos"
        ext = "tar.gz"
        nstrip = 3
    elif osys.is_linux:
        plat = "linux"
        ext = "tar.gz"
        nstrip = 1
    else:
        raise NotImplementedError(f"No automatic download implemented for operating system {osys=}")

    if osys.is_darwin:
        arch = "universal"
    elif arch is Architecture.x86_64:
        arch = "x86_64"
    elif arch is Architecture.ARM64:
        if osys.is_linux:
            arch = "aarch64"
        elif osys.is_windows:
            arch = "arm64"
        else:
            assert False
    else:
        raise NotImplementedError(
            f"No automatic download implemented for operating system+architecture {osys=}, {arch=}"
        )
    vermat = re.match(r"^(\d+)\.(\d+)", version)
    assert vermat, f"Unvalid version string? {version=}"
    major, minor = vermat.groups()
    url = f"https://cmake.org/files/v{major}.{minor}/cmake-{version}-{plat}-{arch}.{ext}"
    ui.status(f"Downloading prebuilt CMake [{url}]")
    async with http.download_tmp(url, suffix=f"-cmake.{ext}") as tmp:
        ui.print(f"Downloaded temporary file [{tmp}]")
        await ar.expand(
            tmp,
            destination=install_root,
            strip_components=nstrip,
            if_exists="replace",
            on_extract="print-status",
        )
    return Installation(install_root)


async def _build_from_source(version, install_root) -> Installation:
    raise NotImplementedError

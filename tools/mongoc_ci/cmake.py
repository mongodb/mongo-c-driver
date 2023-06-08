from __future__ import annotations

import itertools
import multiprocessing
import os
import re
from pathlib import Path
from typing import Iterable, Mapping

from dagon import ar, fs, http, proc, task, ui

from .ninja import ninja_progress
from .paths import EXE_SUFFIX, OPT_CACHE_DIR
from .platform import Architecture, OperatingSystem

CMakeSettingsMapping = Mapping[str, "str | int | bool | fs.Pathish"]
DEFAULT_PARALLEL = multiprocessing.cpu_count() + 2


class CMakeExecutable:
    """
    A handle to a CMake executable and methods to control it.

    :param exe: The CMake executable to wrap.
    """

    def __init__(self, exe: Path) -> None:
        self.executable = exe

    async def configure(
        self,
        *,
        source_dir: fs.Pathish,
        build_dir: fs.Pathish,
        generator: str | None = None,
        settings: None | CMakeSettingsMapping = None,
        install_prefix: fs.Pathish | None = None,
        config: str | None = None,
        on_output: proc.OutputMode = "print",
    ) -> None:
        """
        Configure a CMake project using this CMake executable.

        :param source_dir: The source directory of the project to configure
        :param build_dir: The directory where the configuration and build results
            will live.
        :param generator: If provided, set the '-G' parameter when configuring.
        :param settings: A mapping of CMake settings to setting values. This will
            generate the '-D' arguments to the CMake configure command.
        :param install_prefix: Set the ``CMAKE_INSTALL_PREFIX`` setting for the
            build.
        :param config: Set the ``CMAKE_BUILD_TYPE`` for the build. Not respected
            by multi-config generators, which require config to be specified
            at build-time.
        """
        # Ensure the build directory exists
        Path(build_dir).mkdir(exist_ok=True, parents=True)
        cmd = (
            # First argument is the source directory path:
            (self.executable, source_dir),
            # Pass a -G argument, if provided
            ("-G", generator) if generator else (),
            # Set the build type
            ("-D", f"CMAKE_BUILD_TYPE={config}") if config else (),
            # Set the CMake settings:
            (("-D", f"{key}={val}") for key, val in settings.items()) if settings else (),
            # Set the install prefix:
            ("-D", f"CMAKE_INSTALL_PREFIX={install_prefix}") if install_prefix else (),
        )
        await proc.run(cmd, on_output=on_output, cwd=build_dir)

    async def build(
        self,
        build_dir: fs.Pathish,
        *,
        config: str | None = None,
        targets: Iterable[str] = (),
        parallel: int = DEFAULT_PARALLEL,
    ) -> None:
        """
        Execute a CMake build for the given configured project.

        :param build_dir: The directory containing a project configuration.
        :param config: The config to build (Only used by multi-config generators)
        :param targets: If provided, builds only these targets.
        :param parallel: Set the ammount of parallelism for the build. Default is
            the number of cores on the system, plus two.
        """
        if isinstance(targets, str):
            targets = [targets]
        cmd = [
            (self.executable, "--build", build_dir),
            (f"--config={config}" if config else ()),
            f"--parallel={parallel}",
            (f"--target={t}" for t in targets),
        ]
        await proc.run(cmd, on_output=ninja_progress)

    async def install(self, build_dir: Path, *, config: str | None = None, prefix: fs.Pathish | None = None) -> None:
        """
        Install a configured and built CMake project.

        :param build_dir: The path for a built project.
        :param config: The configuration to install (Only useful to multi-config generators)
        :param prefix: Set the CMAKE_INSTALL_PREFIX. If unspecified, the install prefix
            that was set at configure-time will be used.
        """
        if prefix:
            prefix = Path(prefix)
            if not prefix.is_absolute():
                raise ValueError(f"Installation prefix must be an absolute path (Got “{prefix}”)")
        cmd = [
            self.executable,
            f"-DCMAKE_INSTALL_CONFIG_NAME={config}" if config else (),
            f"-DCMAKE_INSTALL_PREFIX={prefix}" if prefix else (),
            ("-P", build_dir / "cmake_install.cmake"),
        ]
        await proc.run(cmd, on_output="status")


class Installation:
    """
    Represents a CMake installation prefix and the components therein

    :param root: The prefix of the CMake installation.
    """

    def __init__(self, root: Path) -> None:
        self.root = root

    @property
    def cmake(self) -> CMakeExecutable:
        """The CMake executable contained in this installation"""
        return CMakeExecutable(self.root / f"bin/cmake{EXE_SUFFIX}")


async def get_cached_cmake_installation(version: str, *, cache_root: Path | None = None) -> Installation:
    """
    Obtain and cache a CMake installation for the specified version.
    """
    cache_root = cache_root or OPT_CACHE_DIR.get()
    install_root = cache_root / f"cmake-{version}"
    cmake_exe = install_root / f"bin/cmake{EXE_SUFFIX}"
    if cmake_exe.is_file():
        return Installation(install_root)

    # Generate a new CMake installation at a temporary location, which we will
    # then relocate to the final location:
    install_tmp = install_root.with_suffix(".tmp")
    removal = fs.remove([install_tmp, install_root], recurse=True, absent_ok=True)
    task.cleanup(lambda: removal, when="now")

    osys = OperatingSystem.current()
    if osys.is_windows or osys.is_darwin:
        # Windows and macOS both have prebuilt binaries for any platform we need:
        tmp_install = await _download_prebuilt(version, install_tmp)
    elif (
        # Linux is trickier:
        osys.is_linux
        # CMake.org only publishes x64 and arm64 Linux binaries
        and Architecture.current() in (Architecture.x86_64, Architecture.ARM64)
        # CMake.org does not publish a binary compatible with Alpine (libmuslc)
        and not Path("/etc/alpine-release").is_file()
    ):
        tmp_install = await _download_prebuilt(version, install_tmp)
    else:
        # No other option: Build CMake from source:
        tmp_install = await _build_from_source(version, install_tmp)
    # Move the temporary install to the final location:
    tmp_install.root.rename(install_root)
    # This is our result:
    return Installation(install_root)


async def _download_prebuilt(version: str, install_root: Path) -> Installation:
    """Download a pre-built version of CMake from cmake.org"""
    osys = OperatingSystem.current()
    arch = Architecture.current()
    ext = "tar.gz"
    nstrip = 1
    exc = NotImplementedError(f"No automatic download implemented for operating system+architecture {osys=}, {arch=}")
    if osys.is_windows:
        plat = "windows"
        # Windows comes in a Zip
        ext = "zip"
    elif osys.is_darwin:
        plat = "macos"
        # macOS has an app bundle, with additional control directories to skip:
        nstrip = 3
    elif osys.is_linux:
        plat = "linux"
    else:
        raise exc

    if osys.is_darwin:
        # macOS has a universal binary
        arch = "universal"
    elif arch is Architecture.x86_64:
        arch = "x86_64"
    elif arch is Architecture.ARM64:
        if osys.is_linux:
            arch = "aarch64"
        elif osys.is_windows:
            arch = "arm64"
        else:
            raise exc
    else:
        raise exc
    vermat = re.match(r"^(\d+)\.(\d+)", version)
    assert vermat, f"Invalid CMake version string? {version=}"
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


def _cmake_candidates() -> Iterable[Path]:
    """Iterate the candidate CMake executable filepaths on PATH"""
    dirs = os.environ["PATH"].split(os.pathsep)
    if not OperatingSystem.current().is_windows:
        # Most systems just have PATH:
        yield from (Path(d) / "cmake" for d in dirs)
    # Windows uses PATHEXT:
    exts = os.environ["PATHEXT"].split(os.pathsep)
    for dirpath, ext in itertools.product(dirs, exts):
        yield Path(dirpath) / f"cmake{ext}"


def _find_existing_cmake() -> CMakeExecutable | None:
    """Find a CMake executable on PATH"""
    for cand in _cmake_candidates():
        if cand.is_file():
            return CMakeExecutable(cand)


async def _build_from_source(version: str, install_prefix: Path) -> Installation:
    """Build CMake from source and install it to the given prefix. Downloads a source archive."""
    vermat = re.match(r"^(\d+)\.(\d+)", version)
    assert vermat, f"Invalid CMake version string? {version=}"
    major, minor = vermat.groups()
    url = f"https://cmake.org/files/v{major}.{minor}/cmake-{version}.tar.gz"
    ui.status(f"Downloading CMake sources [{url}]")
    src_dir = install_prefix.with_suffix(".src")
    build_dir = install_prefix.with_suffix(".build")
    async with http.download_tmp(url, suffix="-cmake-src.tgz") as tmp:
        await ar.expand(tmp, destination=src_dir, strip_components=1, on_extract="print-status", if_exists="replace")
    task.cleanup(lambda: fs.remove([src_dir, build_dir], recurse=True, absent_ok=True))
    ui.progress(None)
    ui.status(f"Building CMake {version}")
    host_cmake = _find_existing_cmake()
    if host_cmake is not None:
        return await _build_cmake_with_cmake(host_cmake, src_dir, build_dir, install_prefix)
    if not OperatingSystem.current().is_unix_like:
        raise RuntimeError(f"No host CMake was found to perform the CMake bootstrap")
    return await _build_cmake_from_bootstrap(src_dir, build_dir, install_prefix)


async def _build_cmake_from_bootstrap(src_dir: Path, build_dir: Path, install_root: Path) -> Installation:
    """Build CMake using the bootstrap shell script in the source directory"""
    build_dir.mkdir(exist_ok=True, parents=True)
    nprocs = multiprocessing.cpu_count() + 2
    cmd = [
        "/bin/sh",
        src_dir / "bootstrap",
        f"--parallel={nprocs}",
        f"--prefix={install_root}",
    ]
    await proc.run(cmd, cwd=build_dir, on_output="status")
    await proc.run(["make", "-j", nprocs, "install"], on_output=ninja_progress, cwd=build_dir)
    return Installation(install_root)


async def _build_cmake_with_cmake(
    cmake: CMakeExecutable, src_dir: Path, build_dir: Path, install_root: Path
) -> Installation:
    """Build CMake using an already-present version of CMake"""
    # Async-delete any stale config:
    removal = fs.remove(build_dir, recurse=True, absent_ok=True)
    task.cleanup(lambda: removal, when="now")
    build_dir.mkdir(exist_ok=True, parents=True)
    # Configure, build, and install
    await cmake.configure(source_dir=src_dir, build_dir=build_dir, config="Release", install_prefix=install_root)
    await cmake.build(build_dir, config="Release")
    await cmake.install(build_dir, config="Release")
    return Installation(install_root)

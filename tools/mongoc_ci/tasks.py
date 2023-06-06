from pathlib import Path

from dagon import task, option, fs

from . import ninja, cmake, paths


OPT_NINJA_VERSION = option.add("ninja.version", type=str, default="1.10.2")
OPT_CMAKE_VERSION = option.add("cmake.version", type=str, default="3.25.2")
OPT_SOURCE_DIR = option.add(
    "source.dir",
    type=Path,
    default=paths.MONGOC_DIR,
    doc="Path to the CMake project to configure",
)
OPT_BUILD_DIR = option.add(
    "build.dir",
    type=Path,
    default_factory=lambda: OPT_SOURCE_DIR.get() / "_build/default",
    doc="Path to the directory in which to store build results",
)
OPT_BUILD_CONFIG = option.add(
    "build.config",
    type=str,
    default="RelWithDebInfo",
    doc="The configuration to build and install",
)
OPT_INSTALL_DIR = option.add(
    "install.dir",
    type=Path,
    doc="The directory in which to install the build (Required for “cmake.install”)",
    validate=lambda p: None if p.is_absolute() else f"install.dir must be an absolute path (Got “{p}”)",
)


@task.define()
async def cache__clean():
    """
    Delete all cached CI tools
    """
    cache_dir = paths.OPT_CACHE_DIR.get()
    removal = fs.remove(cache_dir, recurse=True, absent_ok=True)
    task.cleanup(lambda: removal, when="now")


@task.define(order_only_depends=[cache__clean])
async def ninja__get() -> Path:
    """Obtain a managed Ninja executable"""
    return await ninja.auto_get_ninja(OPT_NINJA_VERSION.get())


@task.define(order_only_depends=[cache__clean])
async def cmake__get() -> cmake.Installation:
    """Obtain a managed CMake installation"""
    return await cmake.get_cached_cmake_installation(OPT_CMAKE_VERSION.get())


CACHE_WARMUP = task.gather("cache.warmup", [cmake__get, ninja__get], doc="Warm up the tools cache")


@task.define()
async def clean():
    """
    Removes prior build result directory (based on build.dir).

    This task resolves immediately, even though the deletion may take time. The
    files are first moved out-of-the-way, and then deletion begins in the
    background.
    """
    build_dir = OPT_BUILD_DIR.get()
    if build_dir == OPT_SOURCE_DIR.get():
        raise RuntimeError(f"build.dir and source.dir are the same value. Refusing to delete")
    # Begin the removal. This call will move files out of the way, but suspends
    # before performing the slower delete operations:
    removal = fs.remove(build_dir, recurse=True, absent_ok=True)
    # Launch the deletion in the background:
    task.cleanup(lambda: removal, when="now")


@task.define(order_only_depends=[clean], depends=[ninja__get, cmake__get])
async def cmake__configure() -> Path:
    """
    Perform the CMake project configuration.
    """
    build_dir = OPT_BUILD_DIR.get()
    cm = await task.result_of(cmake__get)
    ninja_exe = await task.result_of(ninja__get)
    await cm.cmake.configure(
        source_dir=OPT_SOURCE_DIR.get(),
        build_dir=build_dir,
        generator="Ninja Multi-Config",
        settings={
            "CMAKE_MAKE_PROGRAM": ninja_exe,
        },
        install_prefix=OPT_INSTALL_DIR.get(default=None),
    )
    return build_dir


@task.define(depends=[cmake__get], order_only_depends=[cmake__configure])
async def cmake__build_fast():
    """
    Build the CMake project (No [cmake.config] dep).
    """
    cm = await task.result_of(cmake__get)
    await cm.cmake.build(OPT_BUILD_DIR.get(), config=OPT_BUILD_CONFIG.get())


cmake__build = task.gather(
    "cmake.build",
    [cmake__build_fast, cmake__configure],
    doc="Configure and build the CMake project",
)


@task.define(depends=[cmake__get], order_only_depends=[cmake__build_fast])
async def cmake__install_fast():
    """
    Install the build results (No [cmake.build] dep)
    """
    cm = await task.result_of(cmake__get)
    await cm.cmake.install(OPT_BUILD_DIR.get(), config=OPT_BUILD_CONFIG.get(), prefix=OPT_INSTALL_DIR.get())


cmake__install = task.gather(
    "cmake.install",
    [cmake__install_fast, cmake__build],
    doc="Install the CMake build results",
)

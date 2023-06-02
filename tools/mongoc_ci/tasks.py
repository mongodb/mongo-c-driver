from pathlib import Path
import multiprocessing

from dagon import task, proc, option, fs

from . import ninja, cmake, paths

THIS_FILE = Path(__file__).resolve()
MONGOC_DIR = THIS_FILE.parent.parent.parent

OPT_SOURCE_DIR = option.add("source.dir", type=Path, default=MONGOC_DIR)
OPT_BUILD_DIR = option.add("build.dir", type=Path, default=MONGOC_DIR / "_build")
OPT_BUILD_CONFIG = option.add("build.config", type=Path, default="Debug")
OPT_NINJA_VERSION = option.add("ninja.version", type=str, default="1.10.2")
OPT_CMAKE_VERSION = option.add("cmake.version", type=str, default="3.25.2")


@task.define()
async def cache__clean():
    """
    Delete all cached CI tools
    """
    cache_dir = paths.OPT_CACHES_DIR.get()
    removal = fs.remove(cache_dir, recurse=True, absent_ok=True)
    task.cleanup(lambda: removal, when="now")


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


@task.define(order_only_depends=[cache__clean])
async def ninja__get() -> Path:
    return await ninja.auto_get_ninja(OPT_NINJA_VERSION.get())


@task.define(order_only_depends=[cache__clean])
async def cmake__get() -> cmake.Installation:
    return await cmake.get_cmake_installation(OPT_CMAKE_VERSION.get())


@task.define(order_only_depends=[clean], depends=[ninja__get, cmake__get])
async def cmake__configure() -> Path:
    """
    Perform the CMake project configuration.
    """
    env = {}
    build_dir = OPT_BUILD_DIR.get() / "default"
    config = OPT_BUILD_CONFIG.get()
    cm = await task.result_of(cmake__get)
    ninja_exe = await task.result_of(ninja__get)
    await proc.run(
        [
            cm.cmake.executable,
            ("-B", build_dir),
            ("-S", MONGOC_DIR),
            ("-GNinja Multi-Config"),
            ("-D", f"CMAKE_BUILD_TYPE={config}"),
            ("-D", f"CMAKE_MAKE_PROGRAM={ninja_exe}"),
        ],
        on_output="print",
    )
    return build_dir


@task.define(depends=[cmake__configure, cmake__get])
async def cmake__build():
    """
    Build the CMake project.
    """
    build_dir = await task.result_of(cmake__configure)
    cm = await task.result_of(cmake__get)
    config = OPT_BUILD_CONFIG.get()
    nproc = multiprocessing.cpu_count() + 2
    await proc.run(
        [cm.cmake.executable, "--build", build_dir, f"--config={config}", f"--parallel={nproc}"],
        on_output=ninja.ninja_progress,
    )

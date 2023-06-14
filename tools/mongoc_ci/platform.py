from __future__ import annotations
from enum import Enum

import platform as py_platform
import sys

_plat = sys.platform
_machine = py_platform.machine()


class OperatingSystem(Enum):
    Windows = "windows"
    Linux = "linux"
    Darwin = "darwin"
    Unknown = "unknown"

    if _plat == "win32":
        Current = Windows
    elif _plat == "linux":
        Current = Linux  # pyright: ignore
    elif _plat == "darwin":
        Current = Darwin
    else:
        Current = Unknown

    @property
    def is_windows(self) -> bool:
        return self is self.Windows

    @property
    def is_darwin(self) -> bool:
        return self is self.Darwin

    @property
    def is_linux(self) -> bool:
        return self is self.Linux

    @property
    def is_unix_like(self) -> bool:
        return self in (self.Linux, self.Darwin)


class Architecture(Enum):
    x86 = "x86"
    x86_64 = "x86_64"
    ARM64 = "arm64"
    PowerPC = "ppc"
    zSeries = "s390x"
    Unknown = "unknown"

    @property
    def is_arm64(self) -> bool:
        return self is Architecture.ARM64

    @property
    def is_x86(self) -> bool:
        return self in (Architecture.x86, Architecture.x86_64)

    if _machine in ("i386", "i686"):
        Current = x86
    elif _machine.lower() in ("amd64", "x86_64"):
        Current = x86_64  # pyright: ignore
    elif _machine.lower() in ("aarch64", "arm64"):
        Current = ARM64
    elif _machine.lower() in ("ppc64le", "ppc64", "ppc"):
        Current = PowerPC
    elif _machine == "s390x":
        Current = zSeries
    else:
        Current = Unknown

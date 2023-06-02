from __future__ import annotations
from enum import Enum

import platform as py_platform
import sys


class OperatingSystem(Enum):
    Windows = "windows"
    Linux = "linux"
    Darwin = "darwin"
    Unknown = "unknown"

    @staticmethod
    def current() -> OperatingSystem:
        plat = sys.platform
        if plat == "win32":
            return OperatingSystem.Windows
        elif plat == "linux":
            return OperatingSystem.Linux
        elif plat == "darwin":
            return OperatingSystem.Darwin
        return OperatingSystem.Unknown

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
    Unknown = "unknown"

    @staticmethod
    def current() -> Architecture:
        mac = py_platform.machine()
        if mac == "i386":
            return Architecture.x86
        elif mac in ("AMD64", "x86_64"):
            return Architecture.x86_64
        elif mac in ("aarch64", "ARM64"):
            return Architecture.ARM64
        else:
            return Architecture.Unknown

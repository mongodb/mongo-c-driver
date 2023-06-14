import enum
from typing import Callable, Final, Iterable, Literal, NamedTuple, Sequence
from config_generator.etc.utils import Task
from config_generator.etc import distros


from shrub.v3.evg_command import subprocess_exec, BuiltInCommand, EvgCommandType
from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef


def poetry_command(cmd: Iterable[str], command_type: EvgCommandType) -> BuiltInCommand:
    return subprocess_exec(
        binary="bash", args=["tools/poetry.sh", *cmd], working_dir="mongoc", command_type=command_type
    )


def tasks() -> Sequence[Task]:
    return [
        Task(
            name="dagon-build",
            commands=[
                poetry_command(["--env-use"], EvgCommandType.SYSTEM),
                poetry_command(["install"], EvgCommandType.SETUP),
                poetry_command(["run", "dagon", "cmake.build"], EvgCommandType.TEST),
            ],
            tags=["dagon"],
        )
    ]


DistSelector = Callable[[distros.Distro], bool]


class _NotSpecifiedType(enum.Enum):
    Const = 0


NotSpecified: Final = _NotSpecifiedType.Const


def select(
    *,
    name: str | _NotSpecifiedType = NotSpecified,
    os: str | None | _NotSpecifiedType = NotSpecified,
    os_ver: str | None | _NotSpecifiedType = NotSpecified,
    os_type: str | None | _NotSpecifiedType = NotSpecified,
    vs_ver: distros.VSVersionString | None | _NotSpecifiedType = NotSpecified,
    size: Literal["small", "large", None, NotSpecified] = NotSpecified,
    arch: Literal["arm64", "power8", "zseries", None, NotSpecified] = NotSpecified,
) -> DistSelector:
    return lambda d: all(
        [
            (d.name == name or name is NotSpecified),
            (d.os == os or os is NotSpecified),
            (d.os_ver == os_ver or os_ver is NotSpecified),
            (d.os_type == os_type or os_type is NotSpecified),
            (d.vs_ver == vs_ver or vs_ver is NotSpecified),
            (d.size == size or size is NotSpecified),
            (d.arch == arch or arch is NotSpecified),
        ]
    )


VARIANT_DISTROS: Sequence[tuple[str, str, DistSelector]] = [
    ("Debian 8.1 x86_64", "deb8.1-x64", select(os="debian", os_ver="8.1", arch=None)),
    ("Debian 9.2 x86_64", "deb9.2-x64", select(os="debian", os_ver="9.2", arch=None)),
    ("Debian 10 x86_64", "deb10-x64", select(os="debian", os_ver="10", arch=None)),
    ("Debian 11 x86_64", "deb11-x64", select(os="debian", os_ver="11", arch=None)),
    ("RHEL 7.0 x86_64", "rh7.0-x64", select(os="rhel", os_ver="7.0", arch=None)),
    ("RHEL 7.6 x86_64", "rh7.6-x64", select(os="rhel", os_ver="7.6", arch=None)),
    ("RHEL 8.0 x86_64", "rh8.0-x64", select(os="rhel", os_ver="8.0", arch=None)),
    ("RHEL 8.4 x86_64", "rh8.4-x64", select(os="rhel", os_ver="8.4", arch=None)),
    ("RHEL 9.0 x86_64", "rh9.0-x64", select(os="rhel", os_ver="9.0", arch=None)),
    ("RHEL 8.2 ARM64", "rh8.2-a64", select(os="rhel", os_ver="8.2", arch="arm64")),
    ("RHEL 9.0 ARM64", "rh9.0-a64", select(os="rhel", os_ver="9.0", arch="arm64")),
    ("RHEL 8.1 PowerPC", "rh8.1-ppc64le", select(os="rhel", os_ver="8.1", arch="power8")),
    ("RHEL 7.2 zSeries", "rh7.2-s390x", select(os="rhel", os_ver="7.2", arch="zseries")),
    ("RHEL 8.3 zSeries", "rh8.3-s390x", select(os="rhel", os_ver="8.3", arch="zseries")),
    ("Ubuntu 18.04 x86_64", "u18-x64", select(os="ubuntu", os_ver="18.04", arch=None)),
    ("Ubuntu 20.04 x86_64", "u20-x64", select(os="ubuntu", os_ver="20.04", arch=None)),
    ("Ubuntu 22.04 x86_64", "u22-x64", select(os="ubuntu", os_ver="22.04", arch=None)),
    ("Ubuntu 18.04 ARM64", "u18-a64", select(os="ubuntu", os_ver="18.04", arch="arm64")),
    ("Ubuntu 20.04 ARM64", "u20-a64", select(os="ubuntu", os_ver="20.04", arch="arm64")),
    ("Ubuntu 22.04 ARM64", "u22-a64", select(os="ubuntu", os_ver="22.04", arch="arm64")),
    ("Ubuntu 18.04 zSeries", "u18-s390x", select(os="ubuntu", os_ver="18.04", arch="zseries")),
    # macOS
    ("macOS 10.14 x86_64", "macos10.14-x64", select(os="macos", os_ver="10.14", arch=None)),
    ("macOS 11.00 x86_64", "macos11.00-x64", select(os="macos", os_ver="11.00", arch=None)),
    ("macOS 11.00 ARM64", "macos11.00-a64", select(os="macos", os_ver="11.00", arch="arm64")),
    # At time of writing (2023-06-13), macos-1015 and macos-1012 are quarantined:
    # ("macOS 10.12 x86_64", "macos10.12-x64", select(os="macos", os_ver="10.12", arch=None)),
    # ("macOS 10.15 x86_64", "macos10.15-x64", select(os="macos", os_ver="10.15", arch=None)),
]


class VariantBasis(NamedTuple):
    basename: str
    slug: str
    distros: Sequence[distros.Distro]


def gen_variant_bases() -> Iterable[VariantBasis]:
    for name, slug, select in VARIANT_DISTROS:
        ds = list(filter(select, distros.ALL_DISTROS))
        yield VariantBasis(name, slug, ds)


def gen_variants() -> Iterable[BuildVariant]:
    for name, slug, select in VARIANT_DISTROS:
        dists = list(filter(select, distros.ALL_DISTROS))
        assert dists, f'Distro selector did not find anything for "{name}"'
        yield BuildVariant(
            name=slug,
            display_name=name,
            run_on=list(d.name for d in dists),
            tasks=[EvgTaskRef(name=".dagon")],
        )


def variants() -> Sequence[BuildVariant]:
    return list(gen_variants())

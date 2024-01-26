from __future__ import annotations

import functools
import itertools
from typing import Any, Iterable, Literal, TypeVar, get_args, NamedTuple, get_type_hints
from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef
from shrub.v3.evg_command import subprocess_exec, EvgCommandType

from config_generator.etc.utils import Task

T = TypeVar("T")

# Identifiers for environments. These correspond to special '*-env' targets in the Earthfile.
EnvKey = Literal["ubuntu2204", "alpine3.18", "archlinux"]

# A mapping from environment keys to 'pretty' environment names.
_ENV_NAMES: dict[EnvKey, str] = {
    "ubuntu2204": "Ubuntu 22.04",
    "alpine3.18": "Alpine 3.18",
    "archlinux": "Arch Linux",
}

# Other options.
# Valid options for the SASL configuration parameter.
SASLOption = Literal["cyrus", "off"]
# Options for the TLS backend configuration parameter (aka 'ENABLE_SSL').
TLSOption = Literal["libressl", "openssl", "off"]
# C++ Driver versions that are under CI test.
CxxVersion = Literal["master", "r3.8.0"]


class Configuration(NamedTuple):
    """
    Represent a complete set of argument values to give to the Earthly task
    execution. Each field name matches the ARG in the Earthfile.

    Adding/removing fields will add/remove dimensions on the task matrix.

    The 'env' parameter is not encoded here, but is managed separately.

    Keep this in sync with the 'PartialConfiguration' class defined below!
    """

    sasl: SASLOption
    tls: TLSOption
    version: CxxVersion

    @classmethod
    def all(cls) -> Iterable[Configuration]:
        """
        Generate all configurations for all options of our parameters.
        """
        # Iter each configuration parameter:
        fields: Iterable[tuple[str, type]] = get_type_hints(
            Configuration).items()
        # Generate lists of pairs of parameter names their options:
        all_pairs: Iterable[Iterable[tuple[str, str]]] = (
            # Generate a (key, opt) pair for each option in parameter 'key'
            [(key, opt) for opt in get_args(typ)]
            # Over each parameter and type thereof:
            for key, typ in fields
        )
        # Now generate the cross product of all alternative for all options:
        matrix: Iterable[dict[str, Any]] = map(
            dict, itertools.product(*all_pairs))
        for items in matrix:
            # Convert each item to a Configuration:
            yield Configuration(**items)

    @property
    def suffix(self) -> str:
        return "-".join(f"{k}-{v}" for k, v in self._asdict().items())


def task_filter(env: EnvKey, conf: Configuration) -> bool:
    """
    Control which tasks are actually defined by matching on the platform and
    configuration values.
    """
    match env, conf:
        # We only need one task with "sasl=off"
        case "ubuntu2204", ("off", "openssl", "master"):
            return True
        # Other sasl=off tasks we'll just ignore:
        case _, ("off", _tls, _cxx):
            return False
        # Ubuntu does not ship with a LibreSSL package:
        case e, (_sasl, "libressl", _cxx) if e.startswith("ubuntu"):
            return False
        # Run all other combinations.
        case _:
            return True


def envs_for(config: Configuration) -> Iterable[EnvKey]:
    """Get all environment keys that are not excluded for the given configuration"""
    all_envs: tuple[EnvKey, ...] = get_args(EnvKey)
    allow_env_for_config = functools.partial(task_filter, conf=config)
    return filter(allow_env_for_config, all_envs)


def earthly_task(
    *,
    name: str,
    targets: Iterable[str],
    config: Configuration,
    env: EnvKey,
) -> Task | None:
    # Generate the build-arg arguments based on the configuration options. The
    # NamedTuple field names must match with the ARG keys in the Earthfile!
    earthly_args = " ".join(f"--{key}={val}" for key, val in config._asdict().items())
    return Task(
        name=name,
        commands=[
            # First, just build the "env-warmup" which will prepare the build environment.
            # This won't generate any output, but allows EVG to track it as a separate build step
            # for timing and logging purposes. The subequent build step will cache-hit the
            # warmed-up build environments.
            subprocess_exec(
                "bash",
                args=[
                    "-c",
                    f"./tools/earthly.sh +env-warmup --env={env} {earthly_args}",
                ],
                working_dir="mongoc",
                command_type=EvgCommandType.SETUP,
            ),
            # Now execute the main tasks:
            subprocess_exec(
                "bash",
                args=[
                    "-c",
                    f"./tools/earthly.sh +run --targets=\"{' '.join(targets)}\" --env={env} {earthly_args}",
                ],
                working_dir="mongoc",
                command_type=EvgCommandType.TEST,
            ),
        ],
        tags=["earthly", "pr-merge-gate", f"earthly-{env}"],
    )


CONTAINER_RUN_DISTROS = [
    "ubuntu2204-small",
    "ubuntu2204-large",
    "ubuntu2004-small",
    "ubuntu2004",
    "ubuntu1804",
    "ubuntu1804-medium",
    "debian10",
    "debian11",
    "amazon2",
]


def tasks() -> Iterable[Task]:
    for conf in Configuration.all():
        for env in envs_for(conf):
            yield earthly_task(
                name=f"earthly-{env}-{conf.suffix}",
                targets=("test-example", "test-cxx-driver"),
                config=conf,
                env=env,
            )


def variants() -> list[BuildVariant]:
    return [
        BuildVariant(
            name="earthly",
            tasks=[EvgTaskRef(name=".earthly")],
            display_name="earthly",
            # Earthly tasks do not depend on a specific EVG distro.
            run_on=CONTAINER_RUN_DISTROS,
        )
    ]

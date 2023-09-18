from typing import Iterable
from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef
from ..etc.utils import Task
from shrub.v3.evg_command import subprocess_exec, EvgCommandType

_ENV_PARAM_NAME = "MONGOC_EARTHLY_ENV"


class EarthlyTask(Task):
    def __init__(self, *, suffix: str, target: str, sasl: str) -> None:
        super().__init__(
            name=f"earthly-{suffix}",
            commands=[
                subprocess_exec(
                    "bash",
                    args=[
                        "tools/earthly.sh",
                        f"+{target}",
                        f"--env=${{{_ENV_PARAM_NAME}}}",
                        f"--enable_sasl={sasl}",
                    ],
                    working_dir="mongoc",
                    command_type=EvgCommandType.TEST,
                )
            ],
            tags=[f"earthly", "pr-merge-gate"],
            run_on=CONTAINER_RUN_DISTROS,
        )


#: A mapping from environment keys to the environment name.
#: These correspond to special "*-env" targets in the Earthfile.
ENVS = {
    "u22": "Ubuntu 22.04",
    "alpine3.18": "Alpine 3.18",
    "archlinux": "Arch Linux",
}

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

# Other options: SSPI (Windows only), AUTO (not reliably test-able without more environments)
_SASL_OPTIONS = ["CYRUS", "OFF"]


def tasks() -> Iterable[Task]:
    for sasl in _SASL_OPTIONS:
        yield EarthlyTask(
            suffix=f"build-check-sasl:{sasl}".lower(),
            target="test-example",
            sasl=sasl,
        )


def variants() -> list[BuildVariant]:
    return [
        BuildVariant(
            name=f"earthly-{env_key}",
            tasks=[EvgTaskRef(name=".earthly")],
            display_name=env_name,
            expansions={
                _ENV_PARAM_NAME: env_key,
            },
        )
        for env_key, env_name in ENVS.items()
    ]

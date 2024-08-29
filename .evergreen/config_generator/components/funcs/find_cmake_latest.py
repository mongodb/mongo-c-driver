from shrub.v3.evg_command import EvgCommandType

from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class FindCMakeLatest(Function):
    name = 'find-cmake-latest'
    command_type = EvgCommandType.SETUP
    commands = [
        bash_exec(
            command_type=command_type,
            retry_on_failure=True,
            working_dir='mongoc',
            script='. .evergreen/scripts/find-cmake-latest.sh && find_cmake_latest'
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return FindCMakeLatest.defn()

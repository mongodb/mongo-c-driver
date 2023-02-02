from shrub.v3.evg_command import EvgCommandType

from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class FetchDET(Function):
    name = 'fetch-det'
    commands = [
        bash_exec(
            command_type=EvgCommandType.SETUP,
            script='''\
                if [[ ! -d drivers-evergreen-tools ]]; then
                    git clone --depth=1 git@github.com:mongodb-labs/drivers-evergreen-tools.git
                fi
            ''',
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return FetchDET.defn()

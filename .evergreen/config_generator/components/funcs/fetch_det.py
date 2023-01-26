from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class FetchDET:
    @classmethod
    def name(cls):
        return 'fetch-det'

    @classmethod
    def defn(cls):
        command_type = EvgCommandType.SETUP

        commands = []

        commands.append(
            bash_exec(
                command_type=command_type,
                script='''\
                    if [[ ! -d drivers-evergreen-tools ]]; then
                        git clone --depth=1 git@github.com:mongodb-labs/drivers-evergreen-tools.git
                    fi
                ''',
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(FetchDET.defn())

    return res

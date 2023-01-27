from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class RunTests:
    @classmethod
    def name(cls):
        return 'run-tests'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                command_type=EvgCommandType.TEST,
                script='.evergreen/scripts/run-tests.sh',
                working_dir='mongoc',
                add_expansions_to_env=True,
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(RunTests.defn())

    return res

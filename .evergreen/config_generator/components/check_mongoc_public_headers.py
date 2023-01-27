from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import FunctionCall
from shrub.v3.evg_task import EvgTask

from config_generator.etc.utils import bash_exec


class CheckMongocPublicHeaders:
    @classmethod
    def name(cls):
        return 'check-headers'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                command_type=EvgCommandType.TEST,
                working_dir='mongoc',
                script='.evergreen/scripts/check-public-decls.sh',
            )
        )

        commands.append(
            bash_exec(
                command_type=EvgCommandType.TEST,
                working_dir='mongoc',
                script='.evergreen/scripts/check-preludes.py .',
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls):
        return FunctionCall(func=cls.name())


def functions():
    res = {}

    res.update(CheckMongocPublicHeaders.defn())

    return res


def tasks():
    res = []

    res.append(
        EvgTask(
            name=CheckMongocPublicHeaders.name(),
            commands=[CheckMongocPublicHeaders.call()],
        )
    )

    return res

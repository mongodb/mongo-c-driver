from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class Backtrace:
    @classmethod
    def name(cls):
        return 'backtrace'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                working_dir='mongoc',
                script='.evergreen/scripts/debug-core-evergreen.sh',
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(Backtrace.defn())

    return res

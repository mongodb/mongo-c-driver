from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class EarlyTermination:
    @classmethod
    def name(cls):
        return 'early-termination'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                script='''\
                    echo 'EVERGREEN HOST WAS UNEXPECTEDLY TERMINATED!!!' 1>&2
                    echo 'Restart this Evergreen task and try again!' 1>&2
                '''
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(EarlyTermination.defn())

    return res

from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class StopMongoOrchestration:
    @classmethod
    def name(cls):
        return 'stop-mongo-orchestration'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                script='''\
                    if [[ -d MO ]]; then
                        cd MO && mongo-orchestration stop
                    fi
                '''
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(StopMongoOrchestration.defn())

    return res

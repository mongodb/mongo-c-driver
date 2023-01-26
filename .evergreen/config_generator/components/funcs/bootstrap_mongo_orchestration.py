from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import expansions_update
from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class BootstrapMongoOrchestration:
    @classmethod
    def name(cls):
        return 'bootstrap-mongo-orchestration'

    @classmethod
    def defn(cls):
        command_type = EvgCommandType.SETUP

        commands = []

        commands.append(
            bash_exec(
                command_type=command_type,
                working_dir='mongoc',
                script='.evergreen/scripts/integration-tests.sh',
                add_expansions_to_env=True,
            )
        )

        commands.append(
            expansions_update(
                command_type=command_type,
                file='mongoc/mo-expansion.yml'
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(BootstrapMongoOrchestration.defn())

    return res

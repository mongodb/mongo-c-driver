from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import ec2_assume_role, expansions_update

from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class BootstrapMongoOrchestration(Function):
    name = 'bootstrap-mongo-orchestration'
    command_type = EvgCommandType.SETUP
    commands = [
        # Downloads of the "latest" server binary require an AWS identity authorized for the
        # private S3 bucket (DRIVERS-3628).
        ec2_assume_role(role_arn='${aws_test_secrets_role}'),
        bash_exec(
            command_type=command_type,
            working_dir='mongoc',
            script='.evergreen/scripts/integration-tests.sh',
            add_expansions_to_env=True,
        ),
        expansions_update(
            command_type=command_type,
            file='mongoc/mo-expansion.yml'
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return BootstrapMongoOrchestration.defn()

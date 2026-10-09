from shrub.v3.evg_command import EvgCommandType, ec2_assume_role

from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class FetchEnterpriseAuthSecrets(Function):
    name = 'fetch-enterprise-auth-secrets'
    commands = [
        ec2_assume_role(role_arn='${aws_test_secrets_role}'),
        bash_exec(
            command_type=EvgCommandType.SETUP,
            working_dir='drivers-evergreen-tools/.evergreen/secrets_handling',
            include_expansions_in_env=[
                'AWS_ACCESS_KEY_ID',
                'AWS_SECRET_ACCESS_KEY',
                'AWS_SESSION_TOKEN',
            ],
            script='./setup-secrets.sh drivers/enterprise_auth',
        ),
    ]


def functions():
    return FetchEnterpriseAuthSecrets.defn()

from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType, ec2_assume_role, KeyValueParam, expansions_update
from shrub.v3.evg_task import EvgTask, EvgTaskRef
from shrub.v3.evg_task_group import EvgTaskGroup

from config_generator.components.funcs.run_tests import RunTests
from config_generator.components.funcs.fetch_det import FetchDET
from config_generator.components.funcs.fetch_source import FetchSource
from config_generator.components.sasl.openssl import SaslCyrusOpenSSLCompile
from config_generator.etc.utils import bash_exec
from config_generator.etc.distros import find_small_distro


def task_groups():
    return [
        EvgTaskGroup(
            name='test-oidc-task-group',
            tasks=['oidc-auth-test-task'],
            setup_group_can_fail_task=True,
            setup_group_timeout_secs=60 * 60,  # 1 hour
            teardown_group_can_fail_task=True,
            teardown_group_timeout_secs=180,  # 3 minutes
            setup_group=[
                FetchDET.call(),
                ec2_assume_role(role_arn='${aws_test_secrets_role}'),
                bash_exec(
                    command_type=EvgCommandType.SETUP,
                    include_expansions_in_env=['AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY', 'AWS_SESSION_TOKEN'],
                    script='./drivers-evergreen-tools/.evergreen/auth_oidc/setup.sh',
                ),
            ],
            teardown_group=[
                bash_exec(
                    command_type=EvgCommandType.SETUP,
                    script='./drivers-evergreen-tools/.evergreen/auth_oidc/teardown.sh',
                )
            ],
        )
    ]


def tasks():
    return [
        EvgTask(
            name='oidc-auth-test-task',
            run_on=[find_small_distro('ubuntu2404').name],
            commands=[
                FetchSource.call(),
                SaslCyrusOpenSSLCompile.call(),
                expansions_update(
                    updates=[
                        KeyValueParam(key='CC', value='gcc'),
                        # OIDC test servers support both OIDC and user/password.
                        KeyValueParam(key='AUTH', value='auth'),  # Use user/password for default test clients.
                        KeyValueParam(key='OIDC', value='oidc'),  # Enable OIDC tests.
                        KeyValueParam(key='MONGODB_VERSION', value='latest'),
                        KeyValueParam(key='TOPOLOGY', value='replica_set'),
                    ]
                ),
                RunTests.call(),
            ],
        )
    ]


def variants():
    return [
        BuildVariant(
            name='oidc-asan',
            display_name='OIDC',
            run_on=[find_small_distro('ubuntu2404').name],
            tasks=[EvgTaskRef(name='test-oidc-task-group')],
            expansions = {
                'ASAN': 'on',
                'CFLAGS': '-fno-omit-frame-pointer',
                'SANITIZE': 'address,undefined',
            }
        ),
    ]

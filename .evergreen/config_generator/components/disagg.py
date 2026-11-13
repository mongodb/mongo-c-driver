from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType, FunctionCall, ec2_assume_role, expansions_update, s3_put
from shrub.v3.evg_task import EvgTask, EvgTaskDependency, EvgTaskRef

from config_generator.components.funcs.csfle_setup import CSFLESetup
from config_generator.components.funcs.csfle_teardown import CSFLETeardown
from config_generator.components.funcs.fetch_build import FetchBuild
from config_generator.components.funcs.fetch_det import FetchDET
from config_generator.components.funcs.run_simple_http_server import RunSimpleHTTPServer
from config_generator.components.funcs.run_tests import RunTests
from config_generator.components.funcs.upload_build import UploadBuild
from config_generator.etc.distros import find_large_distro, make_distro_str
from config_generator.etc.utils import bash_exec

# Every server-side process runs in a container, so only Docker is required on the host.
_DISTRO_NAME = 'ubuntu2404'
_COMPILER = 'gcc'

# Pinned SLS image tag. Resolving the newest mainline tag at run time picked up a build whose services crash-loop
# (all SLS containers stuck in `restarting`); bump deliberately after checking a newer tag comes up healthy.
_SLS_TAG = 'linux-x86-64-f987ae59a6f61e0b4e3edc73663992e15c4e9261'


def functions():
    return {
        'start-dsc': [
            # Requires read access to the SLS ECR repositories. See drivers-evergreen-tools/.evergreen/disagg/README.md.
            ec2_assume_role(role_arn='arn:aws:iam::664315256653:role/disagg-storage-tf-project-ro'),
            bash_exec(
                command_type=EvgCommandType.SETUP,
                include_expansions_in_env=[
                    'AWS_ACCESS_KEY_ID',
                    'AWS_SECRET_ACCESS_KEY',
                    'AWS_SESSION_TOKEN',
                    'DSC_SLS_TAG',
                ],
                working_dir='mongoc',
                script='../drivers-evergreen-tools/.evergreen/disagg/setup.sh',
            ),
            expansions_update(
                command_type=EvgCommandType.SETUP,
                file='mongoc/dsc-expansion.yml',
            ),
        ],
        # Client-side encryption tests need crypt_shared. mongo-orchestration is not bootstrapped, so fetch it directly.
        'fetch-dsc-crypt-shared': [
            # Downloads of the "latest" server binary require an AWS identity (DRIVERS-3628).
            ec2_assume_role(role_arn='${aws_test_secrets_role}'),
            bash_exec(
                command_type=EvgCommandType.SETUP,
                include_expansions_in_env=['AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY', 'AWS_SESSION_TOKEN'],
                working_dir='mongoc',
                script='../drivers-evergreen-tools/.evergreen/disagg/download-crypt.sh',
            ),
            expansions_update(
                command_type=EvgCommandType.SETUP,
                file='mongoc/crypt-shared-expansion.yml',
            ),
        ],
        # Also called from the top-level `post` so logs are collected when tasks fail. Tasks that did not fetch
        # drivers-evergreen-tools did not start a deployment. teardown.sh does nothing if no deployment was started.
        'stop-dsc': [
            bash_exec(
                working_dir='mongoc',
                script="""\
                    if [[ -f ../drivers-evergreen-tools/.evergreen/disagg/teardown.sh ]]; then
                        ../drivers-evergreen-tools/.evergreen/disagg/teardown.sh
                    fi
                """,
            ),
            s3_put(
                aws_key='${aws_key}',
                aws_secret='${aws_secret}',
                bucket='mciuploads',
                content_type='${content_type|application/x-gzip}',
                display_name='sls-logs.txt.gz',
                local_file='mongoc/sls-logs.txt.gz',
                optional=True,
                permissions='public-read',
                remote_file='${project}/${build_variant}/${revision}/${version_id}/${build_id}/logs/${task_id}-${execution}-sls-logs.txt.gz',
            ),
        ],
    }


def tasks():
    distro_str = make_distro_str(_DISTRO_NAME, _COMPILER, None)

    # Full suite: without CLIENT_SIDE_ENCRYPTION=on, run-tests.sh does not limit the tests to run.
    compile_name = f'dsc-{distro_str}-compile'
    yield EvgTask(
        name=compile_name,
        run_on=find_large_distro(_DISTRO_NAME).name,
        tags=['dsc', _DISTRO_NAME, _COMPILER],
        commands=[
            bash_exec(
                command_type=EvgCommandType.TEST,
                env={'CC': _COMPILER, 'CFLAGS': '-fno-omit-frame-pointer', 'SSL': 'OPENSSL'},
                include_expansions_in_env=['distro_id'],
                working_dir='mongoc',
                script='.evergreen/scripts/compile.sh',
            ),
            UploadBuild.call(),
        ],
    )

    yield EvgTask(
        name=f'dsc-{distro_str}-test-replset-3',
        depends_on=[EvgTaskDependency(name=compile_name)],
        run_on=find_large_distro(_DISTRO_NAME).name,
        tags=['dsc', 'replset-3', _DISTRO_NAME, _COMPILER],
        commands=[
            FetchBuild.call(build_name=compile_name),
            FetchDET.call(),
            FunctionCall(
                func='start-dsc',
                vars={'DSC_SLS_TAG': _SLS_TAG},
            ),
            RunSimpleHTTPServer.call(),
            RunTests().call(
                vars={
                    'URI': '${MONGODB_URI}',
                    'DSC': 'on',
                    'CC': _COMPILER,
                }
            ),
        ],
    )

    # Client-side encryption: libmongocrypt is required at compile time, and `CLIENT_SIDE_ENCRYPTION=on` limits
    # run-tests.sh to the CSE and unified tests.
    cse_compile_name = f'dsc-{distro_str}-cse-compile'
    yield EvgTask(
        name=cse_compile_name,
        run_on=find_large_distro(_DISTRO_NAME).name,
        tags=['dsc', 'cse', _DISTRO_NAME, _COMPILER],
        commands=[
            bash_exec(
                command_type=EvgCommandType.TEST,
                env={
                    'CC': _COMPILER,
                    'CFLAGS': '-fno-omit-frame-pointer',
                    'SSL': 'OPENSSL',
                    'COMPILE_LIBMONGOCRYPT': 'ON',
                },
                include_expansions_in_env=['distro_id'],
                working_dir='mongoc',
                script='EXTRA_CONFIGURE_FLAGS="-DENABLE_PIC=ON ${EXTRA_CONFIGURE_FLAGS}" .evergreen/scripts/compile.sh',
            ),
            UploadBuild.call(),
        ],
    )

    yield EvgTask(
        name=f'dsc-{distro_str}-cse-test-replset-3',
        depends_on=[EvgTaskDependency(name=cse_compile_name)],
        run_on=find_large_distro(_DISTRO_NAME).name,
        tags=['dsc', 'cse', 'replset-3', _DISTRO_NAME, _COMPILER],
        commands=[
            FetchBuild.call(build_name=cse_compile_name),
            FetchDET.call(),
            FunctionCall(
                func='start-dsc',
                vars={'DSC_SLS_TAG': _SLS_TAG},
            ),
            FunctionCall(func='fetch-dsc-crypt-shared'),
            CSFLESetup.call(),
            RunTests().call(
                vars={
                    'URI': '${MONGODB_URI}',
                    'CLIENT_SIDE_ENCRYPTION': 'on',
                    'DSC': 'on',
                    'CC': _COMPILER,
                }
            ),
            CSFLETeardown.call(),
        ],
    )


def variants():
    return [
        BuildVariant(
            name='dsc',
            display_name='Disaggregated storage (DSC)',
            tasks=[EvgTaskRef(name='.dsc')],
            # Not yet stable: limit to at most once a day in mainline.
            # TODO: promote to a per-commit variant once stable.
            batchtime=1440,  # 1 day
        ),
    ]

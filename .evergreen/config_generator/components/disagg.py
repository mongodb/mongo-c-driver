from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType, FunctionCall, ec2_assume_role, expansions_update
from shrub.v3.evg_task import EvgTask, EvgTaskDependency, EvgTaskRef

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

# name: (cells, mongods per shard). `slslocal` requires mongods per shard <= cells.
_TOPOLOGIES = {
    'replset-1': (1, 1),
    'replset-3': (3, 3),
}


def functions():
    return {
        'start-dsc': [
            # Requires read access to the SLS ECR repositories. See dsc-evergreen-proposal.md.
            ec2_assume_role(role_arn='${dsc_ecr_role_arn}'),
            bash_exec(
                command_type=EvgCommandType.SETUP,
                include_expansions_in_env=[
                    'AWS_ACCESS_KEY_ID',
                    'AWS_SECRET_ACCESS_KEY',
                    'AWS_SESSION_TOKEN',
                    'DSC_NUM_CELLS',
                    'DSC_MONGODS_PER_SHARD',
                ],
                working_dir='mongoc',
                script='.evergreen/scripts/dsc-start.sh',
            ),
            expansions_update(
                command_type=EvgCommandType.SETUP,
                file='mongoc/dsc-expansion.yml',
            ),
        ],
        # Also called from the top-level `post` so logs are collected when tasks fail.
        'stop-dsc': [
            bash_exec(
                working_dir='mongoc',
                script='.evergreen/scripts/dsc-stop.sh',
            ),
        ],
    }


def tasks():
    distro_str = make_distro_str(_DISTRO_NAME, _COMPILER, None)
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

    for topology, (cells, mongods) in _TOPOLOGIES.items():
        yield EvgTask(
            name=f'dsc-{distro_str}-test-{topology}',
            depends_on=[EvgTaskDependency(name=compile_name)],
            run_on=find_large_distro(_DISTRO_NAME).name,
            tags=['dsc', topology, _DISTRO_NAME, _COMPILER],
            commands=[
                FetchBuild.call(build_name=compile_name),
                FetchDET.call(),
                FunctionCall(
                    func='start-dsc',
                    vars={'DSC_NUM_CELLS': str(cells), 'DSC_MONGODS_PER_SHARD': str(mongods)},
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

from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_task import EvgTask, EvgTaskRef

from config_generator.etc.distros import compiler_to_vars, find_large_distro, make_distro_str
from config_generator.etc.utils import bash_exec

TAG = 'mongoac'

# Building mongoac compiles the Rust `mongodb` driver and its dependencies (~355 crates, including `aws-lc-sys` and
# `zstd-sys`), which is too much work for a `-small` distro.
#
# RHEL 8 x86_64 is the client's baseline platform: it provides the oldest glibc in the distro catalog (2.28), and
# its GCC 8.5 exercises mongoac's documented GCC 8.1 minimum. It is also the baseline of the MongoDB Rust Driver.
MATRIX = [
    ('rhel8-latest', 'gcc', None),
]


def make_task(distro_name, compiler, arch):
    distro_str = make_distro_str(distro_name, compiler, arch)

    tags = [TAG, distro_name, compiler]
    if arch:
        tags.append(arch)

    return EvgTask(
        name=f'{TAG}-{distro_str}',
        run_on=find_large_distro(distro_name).name,  # DEVPROD-18763
        tags=tags,
        exec_timeout_secs=5400,  # The 1-hour project default is not enough for a cargo build of ~355 crates.
        commands=[
            bash_exec(
                command_type=EvgCommandType.TEST,
                add_expansions_to_env=True,
                working_dir='mongoc',
                env=compiler_to_vars(compiler) | {'ENABLE_MONGOAC': 'ON'},
                script='.evergreen/scripts/compile-mongoac.sh',
            ),
            bash_exec(
                command_type=EvgCommandType.TEST,
                working_dir='mongoc',
                script="""\
                    set -o errexit
                    test -x cmake-build/src/libmongoac/test/test-libmongoac
                    test -f cmake-build/src/libmongoac/Debug/libmongoac.so
                """,
            ),
        ],
    )


TASKS = [make_task(*entry) for entry in MATRIX]


def tasks():
    return TASKS


def variants():
    return [
        BuildVariant(
            name=TAG,
            display_name='mongoac',
            tasks=[task.get_task_ref() for task in TASKS],
        ),
    ]

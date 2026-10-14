from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_task import EvgTask

from config_generator.components.funcs.upload_mongoac_test_results import UploadMongoacTestResults
from config_generator.etc.distros import compiler_to_vars, find_large_distro, make_distro_str
from config_generator.etc.utils import bash_exec

TAG = 'mongoac'

# Building mongoac compiles the Rust `mongodb` driver and its dependencies (~355 crates, including `aws-lc-sys` and
# `zstd-sys`), which is too much work for a `-small` distro.
#
# RHEL 8 x86_64 is the client's baseline platform: it provides the oldest glibc among the platforms mongoac is
# tested on (2.28), and its GCC 8.5 exercises mongoac's documented GCC 8.1 minimum. It is also the baseline of the
# MongoDB Rust Driver.
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
            bash_exec(
                command_type=EvgCommandType.TEST,
                add_expansions_to_env=True,
                working_dir='mongoc',
                env=compiler_to_vars(compiler) | {'ENABLE_MONGOAC': 'ON'},
                script="""\
                    # Record the runner's exit status instead of failing the block here:
                    # Evergreen aborts the remaining commands on the first failure, which
                    # would skip the results upload on exactly the runs that need it.
                    if .evergreen/scripts/run-mongoac-tests.sh; then
                      echo 0 > mongoac-test-status
                    else
                      echo $? > mongoac-test-status
                    fi
                """,
            ),
            UploadMongoacTestResults.call(),
            # Validate the documented platform/compiler matrix: the mongoac docs must build with warnings as errors.
            # Run after the tests and the results upload so that a docs error never costs the test results. The script
            # installs its own Rust toolchain and CMake/Ninja, since $PATH does not persist across commands. The
            # `docs` group provides the `sphinx-build` executable required by `find_package (Sphinx REQUIRED)`.
            bash_exec(
                command_type=EvgCommandType.TEST,
                working_dir='mongoc',
                script='uv run --frozen --only-group docs env .evergreen/scripts/build-mongoac-docs.sh',
            ),
            # Report the failure recorded above, after the results have been attached. This runs last so that a test
            # failure still turns the task red; a docs failure turns it red earlier, with the results already
            # attached. A missing status file makes `cat` fail, so this fails closed.
            bash_exec(
                command_type=EvgCommandType.TEST,
                working_dir='mongoc',
                script='test "$(cat mongoac-test-status)" = "0"',
            ),
        ],
    )


TASKS = [make_task(*entry) for entry in MATRIX]


def functions():
    return UploadMongoacTestResults.defn()


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

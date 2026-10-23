from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType, KeyValueParam, expansions_update
from shrub.v3.evg_task import EvgTask

from config_generator.components.funcs.bootstrap_mongo_orchestration import BootstrapMongoOrchestration
from config_generator.components.funcs.fetch_det import FetchDET
from config_generator.components.funcs.set_cache_dir import SetCacheDir
from config_generator.components.funcs.upload_mongoac_test_results import UploadMongoacTestResults
from config_generator.etc.distros import compiler_to_vars, find_large_distro, make_distro_str
from config_generator.etc.utils import bash_exec

TAG = 'mongoac'

MATRIX = [
    ('rhel8-latest', 'gcc', None),
]


def make_task(distro_name: str, compiler: str, arch: str | None) -> EvgTask:
    distro_str = make_distro_str(distro_name, compiler, arch)

    tags = [TAG, distro_name, compiler]
    if arch:
        tags.append(arch)

    return EvgTask(
        name=f'{TAG}-{distro_str}',
        run_on=find_large_distro(distro_name).name,
        tags=tags,
        commands=[
            # Publish the host-persistent cache directory (MONGO_C_DRIVER_CACHE_DIR) so the Rust toolchain is
            # installed there instead of in the checkout, which is wiped between Evergreen tasks.
            SetCacheDir.call(),
            bash_exec(
                command_type=EvgCommandType.TEST,
                add_expansions_to_env=True,
                working_dir='mongoc',
                include_expansions_in_env=['MONGO_C_DRIVER_CACHE_DIR'],
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
                    # The installed shared library must carry an ELF SONAME: the platform matrix in
                    # src/libmongoac/doc/index.rst presents patchelf (which sets it) as part of the tested
                    # baseline, so a build that silently skipped it must not pass.
                    # NB: use the bare `$soname_lib` form, not the brace-delimited form: Evergreen treats
                    # brace-delimited dollar references in inline commands as expansion references.
                    soname_lib="$(find install-dir/lib install-dir/lib64 -maxdepth 1 -name 'libmongoac0.so.*' -type f 2>/dev/null | head -n 1)"
                    test -n "$soname_lib"
                    readelf -d "$soname_lib" | grep -q '(SONAME)' ||
                      { echo "ERROR: $soname_lib has no SONAME (patchelf unavailable?)" 1>&2; exit 1; }
                """,
            ),
            # Start a MongoDB server for the tests that need one: `expansions.update` configures the
            # orchestration (standalone, latest server, no auth, no TLS), `fetch-det` clones
            # drivers-evergreen-tools which the bootstrap function uses, and
            # `bootstrap-mongo-orchestration` starts the server and publishes its connection string as the
            # `MONGODB_URI` expansion. run-mongoac-tests.sh passes that on as `MONGOAC_TEST_URI`.
            expansions_update(
                updates=[
                    KeyValueParam(key='AUTH', value='noauth'),
                    KeyValueParam(key='MONGODB_VERSION', value='latest'),
                    KeyValueParam(key='TOPOLOGY', value='server'),
                    KeyValueParam(key='SSL', value='nossl'),
                ],
            ),
            FetchDET.call(),
            BootstrapMongoOrchestration.call(),
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
            bash_exec(
                command_type=EvgCommandType.TEST,
                working_dir='mongoc',
                include_expansions_in_env=['MONGO_C_DRIVER_CACHE_DIR'],
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

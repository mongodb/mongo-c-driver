from typing import ClassVar

from shrub.v3.evg_command import EvgCommand
from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import expansions_update
from shrub.v3.evg_command import KeyValueParam

from config_generator.etc.distros import find_large_distro
from config_generator.etc.distros import make_distro_str
from config_generator.etc.distros import to_cc
from config_generator.etc.utils import bash_exec
from config_generator.etc.utils import EvgTaskWithRunOn

from config_generator.etc.function import Function

from config_generator.components.funcs.upload_build import UploadBuild


class CompileCommon(Function):
    ssl: ClassVar[str | None]

    @classmethod
    def compile_commands(cls, sasl=None) -> list[EvgCommand]:
        updates = []

        if cls.ssl:
            updates.append(KeyValueParam(key='SSL', value=cls.ssl))

        if sasl:
            updates.append(KeyValueParam(key='SASL', value=sasl))

        return [
            expansions_update(updates=updates),
            # TODO: move into separate task?
            bash_exec(
                command_type=EvgCommandType.TEST,
                script='.evergreen/scripts/compile.sh',
                working_dir='mongoc',
                add_expansions_to_env=True,
                env={
                    'EXTRA_CONFIGURE_FLAGS': '-DENABLE_PIC=ON -DENABLE_MONGOC=OFF',
                },
            ),
            bash_exec(
                command_type=EvgCommandType.TEST,
                script='rm CMakeCache.txt',
                working_dir='mongoc',
            ),
            bash_exec(
                command_type=EvgCommandType.TEST,
                script='.evergreen/scripts/compile.sh',
                working_dir='mongoc',
                add_expansions_to_env=True,
                env={
                    'COMPILE_LIBMONGOCRYPT': 'ON',
                    'EXTRA_CONFIGURE_FLAGS': '-DENABLE_PIC=ON -DENABLE_CLIENT_SIDE_ENCRYPTION=ON',
                },
            ),
        ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, MATRIX):
    res = []

    for distro_name, compiler, arch, sasls, in MATRIX:
        tags = [TAG, 'compile', distro_name, compiler, 'cse']

        distro = find_large_distro(distro_name)

        compile_vars = None
        compile_vars = {'CC': to_cc(compiler)}

        if arch:
            tags.append(arch)
            compile_vars.update({'MARCH': arch})

        distro_str = make_distro_str(distro_name, compiler, arch)

        for sasl in sasls:
            task_name = f'cse-sasl-{sasl}-{SSL}-{distro_str}-compile'

            commands = []
            commands.append(SASL_TO_FUNC[sasl].call(vars=compile_vars))
            commands.append(UploadBuild.call())

            res.append(
                EvgTaskWithRunOn(
                    name=task_name,
                    run_on=distro.name,
                    tags=tags + [f'sasl-{sasl}'],
                    commands=commands,
                )
            )

    return res

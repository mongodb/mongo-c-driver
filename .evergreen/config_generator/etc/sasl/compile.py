from typing import ClassVar

from shrub.v3.evg_command import EvgCommand
from shrub.v3.evg_command import EvgCommandType

from config_generator.etc.distros import find_large_distro
from config_generator.etc.distros import make_distro_str
from config_generator.etc.distros import to_cc
from config_generator.etc.utils import bash_exec
from config_generator.etc.utils import EvgTaskWithRunOn

from config_generator.etc.function import Function

from config_generator.components.funcs.upload_build import UploadBuild


class CompileCommon(Function):
    ssl: ClassVar[str]

    @classmethod
    def compile_commands(cls, sasl=None) -> list[EvgCommand]:
        env = {}

        if cls.ssl:
            env['SSL'] = cls.ssl

        if sasl:
            env['SASL'] = sasl

        return [
            bash_exec(
                command_type=EvgCommandType.TEST,
                add_expansions_to_env=True,
                env=env,
                working_dir='mongoc',
                script='.evergreen/scripts/compile.sh',
            ),
        ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, MATRIX):
    res = []

    for distro_name, compiler, arch, sasls, in MATRIX:
        tags = [TAG, 'compile', distro_name, compiler]

        distro = find_large_distro(distro_name)

        compile_vars = None
        compile_vars = {'CC': to_cc(compiler)}

        if arch:
            tags.append(arch)
            compile_vars.update({'MARCH': arch})

        distro_str = make_distro_str(distro_name, compiler, arch)

        for sasl in sasls:
            task_name = f'sasl-{sasl}-{SSL}-{distro_str}-compile'

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

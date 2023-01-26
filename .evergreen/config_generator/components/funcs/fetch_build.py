from shrub.v3.evg_command import archive_targz_extract
from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import FunctionCall
from shrub.v3.evg_command import s3_get

from config_generator.etc.utils import bash_exec


class FetchBuild:
    @classmethod
    def name(cls):
        return 'fetch-build'

    @classmethod
    def defn(cls):
        command_type = EvgCommandType.SETUP

        commands = []

        commands.append(
            bash_exec(
                command_type=command_type,
                script='rm -rf mongoc',
            )
        )

        commands.append(
            s3_get(
                command_type=command_type,
                aws_key='${aws_key}',
                aws_secret='${aws_secret}',
                bucket='mciuploads',
                local_file='build.tar.gz',
                remote_file='${project}/${build_variant}/${revision}/${BUILD_NAME}/${build_id}.tar.gz',
            )
        )

        commands.append(
            archive_targz_extract(path='build.tar.gz', destination='mongoc')
        )

        # Scripts may not be executable on Windows.
        commands.append(
            bash_exec(
                command_type=command_type,
                working_dir='mongoc',
                script='''\
                    for file in $(find .evergreen/scripts -type f); do
                        chmod +rx "$file" || exit
                    done
                '''
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, build_name):
        return FunctionCall(
            func=cls.name(),
            vars={'BUILD_NAME': build_name}
        )


def functions():
    res = {}

    res.update(FetchBuild.defn())

    return res

from shrub.v3.evg_command import FunctionCall

from shrub.v3.evg_command import archive_targz_pack
from shrub.v3.evg_command import s3_put


class UploadBuild:
    @classmethod
    def name(cls):
        return 'upload-build'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            archive_targz_pack(
                target='${build_id}.tar.gz',
                source_dir='mongoc',
                include=['./**'],
            )
        )

        commands.append(
            s3_put(
                aws_key='${aws_key}',
                aws_secret='${aws_secret}',
                remote_file='${project}/${build_variant}/${revision}/${task_name}/${build_id}.tar.gz',
                bucket='mciuploads',
                permissions='public-read',
                local_file='${build_id}.tar.gz',
                content_type='${content_type|application/x-gzip}',
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls):
        return FunctionCall(func=cls.name())


def functions():
    res = {}

    res.update(UploadBuild.defn())

    return res

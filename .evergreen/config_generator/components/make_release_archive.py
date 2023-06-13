from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import s3_put
from shrub.v3.evg_task import EvgTask

from config_generator.etc.function import Function
from config_generator.etc.function import merge_defns
from config_generator.etc.utils import bash_exec

from config_generator.components.funcs.upload_build import UploadBuild


class ReleaseArchive(Function):
    name = 'release-archive'
    commands = [
        bash_exec(
            command_type=EvgCommandType.TEST,
            working_dir='mongoc',
            env={
                'MONGOC_TEST_FUTURE_TIMEOUT_MS': '30000',
                'MONGOC_TEST_SKIP_LIVE': 'on',
                'MONGOC_TEST_SKIP_SLOW': 'on',
            },
            include_expansions_in_env=['distro_id'],
            script='''\
                set -o errexit
                bash tools/poetry.sh install --with=docs
                bash tools/poetry.sh run \
                    bash .evergreen/scripts/check-release-archive.sh
            '''
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


class UploadDocs(Function):
    name = 'upload-docs'
    commands = [
        bash_exec(
            working_dir='mongoc/cmake_build/src/libbson',
            env={
                'AWS_ACCESS_KEY_ID': '${aws_key}',
                'AWS_SECRET_ACCESS_KEY': '${aws_secret}',
            },
            script='aws s3 cp doc/html s3://mciuploads/${project}/docs/libbson/${CURRENT_VERSION} --quiet --recursive --acl public-read --region us-east-1',
        ),
        s3_put(
            aws_key='${aws_key}',
            aws_secret='${aws_secret}',
            bucket='mciuploads',
            content_type='text/html',
            display_name='libbson docs',
            local_file='mongoc/cmake_build/src/libbson/doc/html/index.html',
            permissions='public-read',
            remote_file='${project}/docs/libbson/${CURRENT_VERSION}/index.html',
        ),
        bash_exec(
            working_dir='mongoc/cmake_build/src/libmongoc',
            env={
                'AWS_ACCESS_KEY_ID': '${aws_key}',
                'AWS_SECRET_ACCESS_KEY': '${aws_secret}',
            },
            script='aws s3 cp doc/html s3://mciuploads/${project}/docs/libmongoc/${CURRENT_VERSION} --quiet --recursive --acl public-read --region us-east-1'
        ),
        s3_put(
            aws_key='${aws_key}',
            aws_secret='${aws_secret}',
            bucket='mciuploads',
            content_type='text/html',
            display_name='libmongoc docs',
            local_file='mongoc/cmake_build/src/libmongoc/doc/html/index.html',
            permissions='public-read',
            remote_file='${project}/docs/libmongoc/${CURRENT_VERSION}/index.html',
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


class UploadManPages(Function):
    name = 'upload-man-pages'
    commands = [
        bash_exec(
            working_dir='mongoc',
            silent=True,
            script='''\
                set -o errexit
                # Get "aha", the ANSI HTML Adapter.
                git clone --depth 1 https://github.com/theZiz/aha.git aha-repo
                pushd aha-repo
                make
                popd # aha-repo
                mv aha-repo/aha .
                .evergreen/scripts/man-pages-to-html.sh libbson cmake_build/src/libbson/doc/man > bson-man-pages.html
                .evergreen/scripts/man-pages-to-html.sh libmongoc cmake_build/src/libmongoc/doc/man > mongoc-man-pages.html
            '''
        ),
        s3_put(
            aws_key='${aws_key}',
            aws_secret='${aws_secret}',
            bucket='mciuploads',
            content_type='text/html',
            display_name='libbson man pages',
            local_file='mongoc/bson-man-pages.html',
            permissions='public-read',
            remote_file='${project}/man-pages/libbson/${CURRENT_VERSION}/index.html',
        ),
        s3_put(
            aws_key='${aws_key}',
            aws_secret='${aws_secret}',
            bucket='mciuploads',
            content_type='text/html',
            display_name='libmongoc man pages',
            local_file='mongoc/mongoc-man-pages.html',
            permissions='public-read',
            remote_file='${project}/man-pages/libmongoc/${CURRENT_VERSION}/index.html',
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


class UploadRelease(Function):
    name = 'upload-release'
    commands = [
        bash_exec(
            script='''\
                if compgen -G "mongoc/cmake_build/mongo*gz" > /dev/null; then
                    mv mongoc/cmake_build/mongo*gz mongoc.tar.gz
                fi
            '''
        ),
        s3_put(
            aws_key='${aws_key}',
            aws_secret='${aws_secret}',
            bucket='mciuploads',
            content_type='${content_type|application/x-gzip}',
            local_file='mongoc.tar.gz',
            permissions='public-read',
            remote_file='${project}/${branch_name}/mongo-c-driver-${CURRENT_VERSION}.tar.gz',
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return merge_defns(
        ReleaseArchive.defn(),
        UploadDocs.defn(),
        UploadManPages.defn(),
        UploadRelease.defn(),
    )


def tasks():
    return [
        EvgTask(
            name='make-release-archive',
            commands=[
                ReleaseArchive.call(),
                UploadDocs.call(),
                UploadManPages.call(),
                UploadBuild.call(),
                UploadRelease.call(),
            ],
        )
    ]

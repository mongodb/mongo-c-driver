from config_generator.etc.distros import find_small_distro
from config_generator.etc.function import Function, merge_defns
from config_generator.etc.utils import bash_exec

from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import BuiltInCommand, EvgCommandType, expansions_update, s3_put
from shrub.v3.evg_task import EvgTask, EvgTaskRef

from pydantic import ConfigDict
from typing import Optional


TAG = 'sbom'


class CustomCommand(BuiltInCommand):
    command: str
    model_config = ConfigDict(arbitrary_types_allowed=True)


def ec2_assume_role(
    role_arn: Optional[str] = None,
    policy: Optional[str] = None,
    duration_seconds: Optional[int] = None,
    command_type: Optional[EvgCommandType] = None,
) -> CustomCommand:
    return CustomCommand(
        command="ec2.assume_role",
        params={
            "role_arn": role_arn,
            "policy": policy,
            "duration_seconds": duration_seconds,
        },
        type=command_type,
    )


class SBOM(Function):
    name = 'sbom'
    commands = [
        ec2_assume_role(
            command_type=EvgCommandType.SETUP,
            role_arn='${kondukto_role_arn}',
        ),
        bash_exec(
            command_type=EvgCommandType.SETUP,
            include_expansions_in_env=[
                'AWS_ACCESS_KEY_ID',
                'AWS_SECRET_ACCESS_KEY',
                'AWS_SESSION_TOKEN',
            ],
            script='''\
                set -o errexit
                set -o pipefail
                kondukto_token="$(aws secretsmanager get-secret-value --secret-id "kondukto-token" --region "us-east-1" --query 'SecretString' --output text)"
                printf "KONDUKTO_TOKEN: %s\\n" "$kondukto_token" >|expansions.kondukto.yml
            ''',
        ),
        expansions_update(
            command_type=EvgCommandType.SETUP,
            file='expansions.kondukto.yml',
        ),
        bash_exec(
            command_type=EvgCommandType.TEST,
            working_dir='mongoc',
            include_expansions_in_env=[
                'artifactory_password',
                'artifactory_username',
                'branch_name',
                'KONDUKTO_TOKEN',
            ],
            script='.evergreen/scripts/sbom.sh',
        ),
        s3_put(
            command_type=EvgCommandType.TEST,
            aws_key='${aws_key}',
            aws_secret='${aws_secret}',
            bucket='mciuploads',
            content_type='application/json',
            display_name='Augmented SBOM',
            local_file='mongoc/augmented-sbom.json',
            permissions='public-read',
            remote_file='mongo-c-driver/${build_variant}/${revision}/${version_id}/${build_id}/sbom/augmented-sbom.json',
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return merge_defns(
        SBOM.defn(),
    )


def tasks():
    distro_name = 'rhel80'
    distro = find_small_distro(distro_name)

    yield EvgTask(
        name='sbom',
        tags=[TAG, distro_name],
        run_on=distro.name,
        commands=[
            SBOM.call(),
        ],
    )


def variants():
    return [
        BuildVariant(
            name=TAG,
            display_name='SBOM',
            tasks=[EvgTaskRef(name=f'.{TAG}')],
        ),
    ]

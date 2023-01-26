from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import expansions_update
from shrub.v3.evg_command import FunctionCall
from shrub.v3.evg_command import git_get_project

from config_generator.etc.utils import bash_exec


class FetchSource:
    @classmethod
    def name(cls):
        return 'fetch-source'

    @classmethod
    def defn(cls):
        command_type = EvgCommandType.SETUP

        commands = []

        commands.append(
            git_get_project(command_type=command_type, directory='mongoc')
        )

        commands.append(
            bash_exec(
                command_type=command_type,
                working_dir='mongoc',
                script='''\
                    set -o errexit
                    if [ -n "${github_pr_number}" -o "${is_patch}" = "true" ]; then
                        # This is a GitHub PR or patch build, probably branched from master
                        if command -v python3 2>/dev/null; then
                            # Prefer python3 if it is available
                            echo $(python3 ./build/calc_release_version.py --next-minor) > VERSION_CURRENT
                        else
                            echo $(python ./build/calc_release_version.py --next-minor) > VERSION_CURRENT
                        fi
                        VERSION=$VERSION_CURRENT-${version_id}
                    else
                        VERSION=latest
                    fi
                    echo "CURRENT_VERSION: $VERSION" > expansion.yml
                '''
            )
        )

        commands.append(
            expansions_update(
                command_type=command_type,
                file='mongoc/expansion.yml'
            )
        )

        commands.append(
            bash_exec(
                command_type=command_type,
                script='''\
                    rm -f *.tar.gz
                    curl --retry 5 --output mongoc.tar.gz -sS --max-time 120 https://s3.amazonaws.com/mciuploads/${project}/${branch_name}/mongo-c-driver-${CURRENT_VERSION}.tar.gz
                '''
            )
        )

        # Scripts may not be executable on Windows.
        commands.append(
            bash_exec(
                command_type=EvgCommandType.SETUP,
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
    def call(cls):
        return FunctionCall(func=cls.name())


def functions():
    res = {}

    res.update(FetchSource.defn())

    return res

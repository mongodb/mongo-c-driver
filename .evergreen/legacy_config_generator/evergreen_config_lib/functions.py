# Copyright 2018-present MongoDB, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from collections import OrderedDict as OD

from evergreen_config_generator.functions import (
    Function, s3_put, shell_exec)
from evergreen_config_lib import shell_mongoc

build_path = '${build_variant}/${revision}/${version_id}/${build_id}'

all_functions = OD([
    ('install ssl', Function(
        shell_mongoc(r'''
        bash .evergreen/scripts/install-ssl.sh
        ''', test=False, add_expansions_to_env=True),
    )),
    ('upload coverage', Function(
        shell_mongoc(r'''
        export AWS_ACCESS_KEY_ID=${aws_key}
        export AWS_SECRET_ACCESS_KEY=${aws_secret}
        aws s3 cp coverage s3://mciuploads/${project}/%s/coverage/ --recursive --acl public-read --region us-east-1
        ''' % (build_path,), test=False, silent=True),
        s3_put(build_path + '/coverage/index.html', aws_key='${aws_key}',
               aws_secret='${aws_secret}',
               local_file='mongoc/coverage/index.html', bucket='mciuploads',
               permissions='public-read', content_type='text/html',
               display_name='Coverage Report'),
    )),
    ('upload scan artifacts', Function(
        shell_mongoc(r'''
        if find scan -name \*.html | grep -q html; then
          (cd scan && find . -name index.html -exec echo "<li><a href='{}'>{}</a></li>" \;) >> scan.html
        else
          echo "No issues found" > scan.html
        fi
        '''),
        shell_mongoc(r'''
        export AWS_ACCESS_KEY_ID=${aws_key}
        export AWS_SECRET_ACCESS_KEY=${aws_secret}
        aws s3 cp scan s3://mciuploads/${project}/%s/scan/ --recursive --acl public-read --region us-east-1
        ''' % (build_path,), test=False, silent=True),
        s3_put(build_path + '/scan/index.html', aws_key='${aws_key}',
               aws_secret='${aws_secret}', local_file='mongoc/scan.html',
               bucket='mciuploads', permissions='public-read',
               content_type='text/html', display_name='Scan Build Report'),
    )),
    # Use "silent=True" to hide output since errors may contain credentials.
    ('run auth tests', Function(
        shell_mongoc(r'''
        bash .evergreen/scripts/run-auth-tests.sh
        ''', add_expansions_to_env=True),
    )),
    ('link sample program', Function(
        shell_mongoc(r'''
        # Compile a program that links dynamically or statically to libmongoc,
        # using variables from pkg-config or CMake's find_package command.
        export BUILD_SAMPLE_WITH_CMAKE=${BUILD_SAMPLE_WITH_CMAKE}
        export BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=${BUILD_SAMPLE_WITH_CMAKE_DEPRECATED}
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  bash .evergreen/scripts/link-sample-program.sh
        LINK_STATIC=1 bash .evergreen/scripts/link-sample-program.sh
        ''',
        include_expansions_in_env=['distro_id']),
    )),
    ('link sample program bson', Function(
        shell_mongoc(r'''
        # Compile a program that links dynamically or statically to libbson,
        # using variables from pkg-config or from CMake's find_package command.
        BUILD_SAMPLE_WITH_CMAKE=  BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=  bash .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=  BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=1 bash .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=  bash .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=1 bash .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1 LINK_STATIC=  bash .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1 LINK_STATIC=1 bash .evergreen/scripts/link-sample-program-bson.sh
        ''',
        include_expansions_in_env=['distro_id']),
    )),
    ('link sample program MSVC', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically or statically to it, using variables from CMake's
        # find_package command.
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        . .evergreen/scripts/use-tools.sh paths
        . .evergreen/scripts/find-cmake-latest.sh
        export CMAKE="$(native-path "$(find_cmake_latest)")"
        LINK_STATIC=  cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc.cmd
        LINK_STATIC=1 cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc.cmd
        ''',
        include_expansions_in_env=['distro_id']),
    )),
    ('link sample program mingw', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically to it, using variables from pkg-config.exe.
        . .evergreen/scripts/use-tools.sh paths
        . .evergreen/scripts/find-cmake-latest.sh
        export CMAKE="$(native-path "$(find_cmake_latest)")"
        cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-mingw.cmd
        ''',
        include_expansions_in_env=['distro_id']),
    )),
    ('link sample program MSVC bson', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically or statically to it, using variables from CMake's
        # find_package command.
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        . .evergreen/scripts/use-tools.sh paths
        . .evergreen/scripts/find-cmake-latest.sh
        export CMAKE="$(native-path "$(find_cmake_latest)")"
        LINK_STATIC=  cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc-bson.cmd
        LINK_STATIC=1 cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc-bson.cmd
        ''',
        include_expansions_in_env=['distro_id']),
    )),
    ('link sample program mingw bson', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically to it, using variables from pkg-config.exe.
        . .evergreen/scripts/use-tools.sh paths
        . .evergreen/scripts/find-cmake-latest.sh
        export CMAKE="$(native-path "$(find_cmake_latest)")"
        cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-mingw-bson.cmd
        '''),
    )),
    ('update codecov.io', Function(
        shell_mongoc(r'''
        # Note: coverage is currently only enabled on the ubuntu1804 distro.
        # This script does not support MacOS, Windows, or non-x86_64 distros.
        # Update accordingly if code coverage is expanded to other distros.
        curl -Os https://uploader.codecov.io/latest/linux/codecov
        chmod +x codecov
        # -Z: Exit with a non-zero value if error.
        # -g: Run with gcov support.
        # -t: Codecov upload token.
        # perl: filter verbose "Found" list and "Processing" messages.
        ./codecov -Zgt "${codecov_token}" | perl -lne 'print if not m|^.*\.gcov(\.\.\.)?$|'
        ''', test=False),
    )),
    ('compile coverage', Function(
        shell_mongoc(r'''
        COVERAGE=ON DEBUG=ON bash .evergreen/scripts/compile.sh
        ''', add_expansions_to_env=True),
    )),
    ('build mongohouse', Function(
        shell_exec(r'''
        cd drivers-evergreen-tools
        export DRIVERS_TOOLS=$(pwd)
        bash .evergreen/atlas_data_lake/build-mongohouse-local.sh
        '''),
    )),
    ('run mongohouse', Function(
        shell_exec(r'''
        cd drivers-evergreen-tools
        export DRIVERS_TOOLS=$(pwd)
        bash .evergreen/atlas_data_lake/run-mongohouse-local.sh
        ''', background=True),
    )),
    ('test mongohouse', Function(
        shell_mongoc(r'''
        echo "Waiting for mongohouse to start..."
        wait_for_mongohouse() {
            for _ in $(seq 300); do
                # Exit code 7: "Failed to connect to host".
                if curl -s localhost:$1; (("$?" != 7)); then
                    return 0
                else
                    sleep 1
                fi
            done
            echo "Could not detect mongohouse on port $1" 1>&2
            return 1
        }
        wait_for_mongohouse 27017 || exit
        echo "Waiting for mongohouse to start... done."
        pgrep -a "mongohouse"
        export RUN_MONGOHOUSE_TESTS=true
        ./src/libmongoc/test-libmongoc --no-fork -l /mongohouse/* -d --skip-tests .evergreen/etc/skip-tests.txt
        unset RUN_MONGOHOUSE_TESTS
        '''),
    )),
    ('run aws tests', Function(
        shell_mongoc(r'''
        # Add AWS variables to a file.
        cat <<EOF > ../drivers-evergreen-tools/.evergreen/auth_aws/aws_e2e_setup.json
        {
            "iam_auth_ecs_account" : "${iam_auth_ecs_account}",
            "iam_auth_ecs_secret_access_key" : "${iam_auth_ecs_secret_access_key}",
            "iam_auth_ecs_account_arn": "arn:aws:iam::557821124784:user/authtest_fargate_user",
            "iam_auth_ecs_cluster": "${iam_auth_ecs_cluster}",
            "iam_auth_ecs_task_definition": "${iam_auth_ecs_task_definition}",
            "iam_auth_ecs_subnet_a": "${iam_auth_ecs_subnet_a}",
            "iam_auth_ecs_subnet_b": "${iam_auth_ecs_subnet_b}",
            "iam_auth_ecs_security_group": "${iam_auth_ecs_security_group}",
            "iam_auth_assume_aws_account" : "${iam_auth_assume_aws_account}",
            "iam_auth_assume_aws_secret_access_key" : "${iam_auth_assume_aws_secret_access_key}",
            "iam_auth_assume_role_name" : "${iam_auth_assume_role_name}",
            "iam_auth_ec2_instance_account" : "${iam_auth_ec2_instance_account}",
            "iam_auth_ec2_instance_secret_access_key" : "${iam_auth_ec2_instance_secret_access_key}",
            "iam_auth_ec2_instance_profile" : "${iam_auth_ec2_instance_profile}",
            "iam_auth_assume_web_role_name": "${iam_auth_assume_web_role_name}",
            "iam_web_identity_issuer": "${iam_web_identity_issuer}",
            "iam_web_identity_rsa_key": "${iam_web_identity_rsa_key}",
            "iam_web_identity_jwks_uri": "${iam_web_identity_jwks_uri}",
            "iam_web_identity_token_file": "${iam_web_identity_token_file}"
        }
        EOF
        ''', silent=True),
        shell_mongoc(r'''
        pushd ../drivers-evergreen-tools/.evergreen/auth_aws
        . ./activate-authawsvenv.sh
        popd # ../drivers-evergreen-tools/.evergreen/auth_aws
        bash .evergreen/scripts/run-aws-tests.sh
        ''', add_expansions_to_env=True)
    )),
    ('start load balancer', Function(
        shell_exec(r'''
        export DRIVERS_TOOLS=./drivers-evergreen-tools
        export MONGODB_URI="${MONGODB_URI}"
        bash $DRIVERS_TOOLS/.evergreen/run-load-balancer.sh start
        ''', test=False),
        OD([
            ("command", "expansions.update"),
            ("params", OD([("file", "lb-expansion.yml")]))
        ]),
    )),
])

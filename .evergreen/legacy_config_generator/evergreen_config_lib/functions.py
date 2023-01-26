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
    Function, s3_put, shell_exec, targz_pack)
from evergreen_config_lib import shell_mongoc

build_path = '${build_variant}/${revision}/${version_id}/${build_id}'

all_functions = OD([
    ('install ssl', Function(
        shell_mongoc(r'''
        bash .evergreen/scripts/install-ssl.sh
        ''', test=False, add_expansions_to_env=True),
    )),
    ('fetch build', Function(
        shell_exec(r'rm -rf mongoc', test=False, continue_on_err=True),
        OD([('command', 's3.get'),
            ('params', OD([
                ('aws_key', '${aws_key}'),
                ('aws_secret', '${aws_secret}'),
                ('remote_file',
                 '${project}/${build_variant}/${revision}/${BUILD_NAME}/${build_id}.tar.gz'),
                ('bucket', 'mciuploads'),
                ('local_file', 'build.tar.gz'),
            ]))]),
        shell_exec(r'''
        mkdir mongoc

        if command -v gtar 2>/dev/null; then
           TAR=gtar
        else
           TAR=tar
        fi

        $TAR xf build.tar.gz -C mongoc/
        ''', test=False, continue_on_err=True),
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
    ('upload mo artifacts', Function(
        shell_mongoc(r'''
        DIR=MO
        [ -d "/cygdrive/c/data/mo" ] && DIR="/cygdrive/c/data/mo"
        [ -d $DIR ] && find $DIR -name \*.log | xargs tar czf mongodb-logs.tar.gz
        ''', test=False),
        s3_put(build_path + '/logs/${task_id}-${execution}-mongodb-logs.tar.gz',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongoc/mongodb-logs.tar.gz', bucket='mciuploads',
               permissions='public-read',
               content_type='${content_type|application/x-gzip}',
               display_name='mongodb-logs.tar.gz'),
        s3_put(build_path + '/logs/${task_id}-${execution}-orchestration.log',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongoc/MO/server.log', bucket='mciuploads',
               permissions='public-read',
               content_type='${content_type|text/plain}',
               display_name='orchestration.log'),
        shell_mongoc(r'''
        # Find all core files from mongodb in orchestration and move to mongoc
        DIR=MO
        MDMP_DIR=$DIR
        [ -d "/cygdrive/c/data/mo" ] && DIR="/cygdrive/c/data/mo"
        [ -d "/cygdrive/c/mongodb" ] && MDMP_DIR="/cygdrive/c/mongodb"
        core_files=$(/usr/bin/find -H $MO $MDMP_DIR \( -name "*.core" -o -name "*.mdmp" \) 2> /dev/null)
        for core_file in $core_files
        do
          base_name=$(echo $core_file | sed "s/.*\///")
          # Move file if it does not already exist
          if [ ! -f $base_name ]; then
            mv $core_file .
          fi
        done
        ''', test=False),
        targz_pack('mongo-coredumps.tgz', 'mongoc', './**.core', './**.mdmp'),
        s3_put(build_path + '/coredumps/${task_id}-${execution}-coredumps.log',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongo-coredumps.tgz', bucket='mciuploads',
               permissions='public-read',
               content_type='${content_type|application/x-gzip}',
               display_name='Core Dumps - Execution ${execution}',
               optional='True'),
    )),
    ('upload working dir', Function(
        targz_pack('working-dir.tar.gz', 'mongoc', './**'),
        s3_put(
            build_path +
            '/artifacts/${task_id}-${execution}-working-dir.tar.gz',
            aws_key='${aws_key}', aws_secret='${aws_secret}',
            local_file='working-dir.tar.gz', bucket='mciuploads',
            permissions='public-read',
            content_type='${content_type|application/x-gzip}',
            display_name='working-dir.tar.gz'),
    )),
    ('upload test results', Function(
        OD([('command', 'attach.results'),
            ('params', OD([
                ('file_location', 'mongoc/test-results.json'),
            ]))]),
    )),
    ('bootstrap mongo-orchestration', Function(
        shell_mongoc(r'''
        export MONGODB_VERSION=${VERSION}
        export TOPOLOGY=${TOPOLOGY}
        export IPV4_ONLY=${IPV4_ONLY}
        export AUTH=${AUTH}
        export AUTHSOURCE=${AUTHSOURCE}
        export SSL=${SSL}
        export ORCHESTRATION_FILE=${ORCHESTRATION_FILE}
        export OCSP=${OCSP}
        export REQUIRE_API_VERSION=${REQUIRE_API_VERSION}
        export LOAD_BALANCER=${LOAD_BALANCER}
        bash .evergreen/scripts/integration-tests.sh
        ''', test=False),
        OD([
            ("command", "expansions.update"),
            ("params", OD([("file", "mongoc/mo-expansion.yml")]))
        ])
    )),
    ('run tests', Function(
        shell_mongoc(r'''
        bash .evergreen/scripts/run-tests.sh
        ''', add_expansions_to_env=True),
    )),
    # Use "silent=True" to hide output since errors may contain credentials.
    ('run auth tests', Function(
        shell_mongoc(r'''
        bash .evergreen/scripts/run-auth-tests.sh
        ''', add_expansions_to_env=True),
    )),
    ('run mock server tests', Function(
        shell_mongoc(r'''
        bash .evergreen/scripts/run-mock-server-tests.sh
        ''', add_expansions_to_env=True),
    )),
    ('cleanup', Function(
        shell_mongoc(r'''
        cd MO
        mongo-orchestration stop
        ''', test=False),
    )),
    ('link sample program', Function(
        shell_mongoc(r'''
        # Compile a program that links dynamically or statically to libmongoc,
        # using variables from pkg-config or CMake's find_package command.
        export BUILD_SAMPLE_WITH_CMAKE=${BUILD_SAMPLE_WITH_CMAKE}
        export BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=${BUILD_SAMPLE_WITH_CMAKE_DEPRECATED}
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  sh .evergreen/scripts/link-sample-program.sh
        LINK_STATIC=1 sh .evergreen/scripts/link-sample-program.sh
        '''),
    )),
    ('link sample program bson', Function(
        shell_mongoc(r'''
        # Compile a program that links dynamically or statically to libbson,
        # using variables from pkg-config or from CMake's find_package command.
        BUILD_SAMPLE_WITH_CMAKE=  BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=  sh .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=  BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=1 sh .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=  sh .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=1 sh .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1 LINK_STATIC=  sh .evergreen/scripts/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1 LINK_STATIC=1 sh .evergreen/scripts/link-sample-program-bson.sh
        '''),
    )),
    ('link sample program MSVC', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically or statically to it, using variables from CMake's
        # find_package command.
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc.cmd
        LINK_STATIC=1 cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc.cmd
        '''),
    )),
    ('link sample program mingw', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically to it, using variables from pkg-config.exe.
        cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-mingw.cmd
        '''),
    )),
    ('link sample program MSVC bson', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically or statically to it, using variables from CMake's
        # find_package command.
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc-bson.cmd
        LINK_STATIC=1 cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-msvc-bson.cmd
        '''),
    )),
    ('link sample program mingw bson', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically to it, using variables from pkg-config.exe.
        cmd.exe /c .\\.evergreen\\scripts\\link-sample-program-mingw-bson.cmd
        '''),
    )),
    ('update codecov.io', Function(
        shell_mongoc(r'''
        export CODECOV_TOKEN=${codecov_token}
        curl -s https://codecov.io/bash | bash
        ''', test=False),
    )),
    ('compile coverage', Function(
        shell_mongoc(r'''
        COVERAGE=ON DEBUG=ON bash .evergreen/scripts/compile.sh
        ''', add_expansions_to_env=True),
    )),
    ('build mongohouse', Function(
        shell_mongoc(r'''
        if [ ! -d "drivers-evergreen-tools" ]; then
           git clone --depth 1 git@github.com:mongodb-labs/drivers-evergreen-tools.git
        fi
        cd drivers-evergreen-tools
        export DRIVERS_TOOLS=$(pwd)

        sh .evergreen/atlas_data_lake/build-mongohouse-local.sh
        cd ../
        '''),
    )),
    ('run mongohouse', Function(
        shell_mongoc(r'''
        cd drivers-evergreen-tools
        export DRIVERS_TOOLS=$(pwd)

        sh .evergreen/atlas_data_lake/run-mongohouse-local.sh
        ''', background=True),
    )),
    ('test mongohouse', Function(
        shell_mongoc(r'''
        echo "testing that mongohouse is running..."
        ps aux | grep mongohouse

        echo $(pwd)
        echo $(ls)

        ls > dir.txt
        cat dir.txt
        echo $(cat dir.txt)

        export RUN_MONGOHOUSE_TESTS=true
        ./src/libmongoc/test-libmongoc --no-fork -l /mongohouse/* -d --skip-tests .evergreen/etc/skip-tests.txt
        unset RUN_MONGOHOUSE_TESTS

        '''),
    )),
    ('test versioned api', Function(
        shell_mongoc(r'''
        MONGODB_API_VERSION=1 bash .evergreen/scripts/run-tests.sh
        ''', add_expansions_to_env=True),
    )),
    ('run aws tests', Function(
        shell_mongoc(r'''
        # Add AWS variables to a file.
        # Clone in one directory above so it does not get uploaded in working directory.
        if [ ! -d ../drivers-evergreen-tools ]; then
            git clone --depth 1 git@github.com:mongodb-labs/drivers-evergreen-tools.git ../drivers-evergreen-tools
        fi
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
            "iam_auth_ec2_instance_profile" : "${iam_auth_ec2_instance_profile}"
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
    ('clone drivers-evergreen-tools', Function(
        shell_exec(r'''
        if [ ! -d "drivers-evergreen-tools" ]; then
            git clone --depth 1 git@github.com:mongodb-labs/drivers-evergreen-tools.git
        fi
        ''', test=False)
    )),
    ('run kms servers', Function(
        shell_exec(r'''
        echo "Preparing CSFLE venv environment..."
        cd ./drivers-evergreen-tools/.evergreen/csfle
        # This function ensures future invocations of activate-kmstlsvenv.sh conducted in
        # parallel do not race to setup a venv environment; it has already been prepared.
        # This primarily addresses the situation where the "run tests" and "run kms servers"
        # functions invoke 'activate-kmstlsvenv.sh' simultaneously.
        # TODO: remove this function along with the "run kms servers" function.
        if [[ "$OSTYPE" =~ cygwin && ! -d kmstlsvenv ]]; then
            # Avoid using Python 3.10 on Windows due to incompatible cipher suites.
            # See CDRIVER-4530.
            . ../venv-utils.sh
            venvcreate "C:/python/Python39/python.exe" kmstlsvenv || # windows-2017
            venvcreate "C:/python/Python38/python.exe" kmstlsvenv    # windows-2015
            python -m pip install --upgrade boto3~=1.19 pykmip~=0.10.0
            deactivate
        else
            . ./activate-kmstlsvenv.sh
            deactivate
        fi
        echo "Preparing CSFLE venv environment... done."
        ''', test=False),
        shell_exec(r'''
        echo "Starting mock KMS servers..."
        cd ./drivers-evergreen-tools/.evergreen/csfle
        . ./activate-kmstlsvenv.sh
        python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/server.pem --port 8999 &
        python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/expired.pem --port 9000 &
        python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/wrong-host.pem --port 9001 &
        python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/server.pem --require_client_cert --port 9002 &
        python -u kms_kmip_server.py &
        deactivate
        echo "Starting mock KMS servers... done."
        ''', test=False, background=True),
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
    ('stop load balancer', Function(
        shell_exec(r'''
        # Only run if a load balancer was started.
        if [ -z "${SINGLE_MONGOS_LB_URI}" ]; then
            echo "OK - no load balancer running"
            exit 0
        fi
        export DRIVERS_TOOLS=./drivers-evergreen-tools
        export MONGODB_URI="foo" # TODO: DRIVERS-1833 remove this.
        $DRIVERS_TOOLS/.evergreen/run-load-balancer.sh stop
        ''', test=False),
    )),
])

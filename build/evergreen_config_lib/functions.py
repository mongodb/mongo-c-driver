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
    ('fetch source', Function(
        OD([('command', 'git.get_project'),
            ('params', OD([
                ('directory', 'mongoc'),
            ]))]),
        shell_mongoc(r'''
        # TODO: CDRIVER-3573 do not hardcode the version.
        if [ -n "${github_pr_number}" -o "${is_patch}" = "true" ]; then
           # This is a GitHub PR or patch build, probably branched from master
           VERSION_CURRENT="1.18.0"
           echo $VERSION_CURRENT > "VERSION_CURRENT"
           VERSION=$VERSION_CURRENT-${version_id}
        else
           VERSION=latest
        fi
        echo "CURRENT_VERSION: $VERSION" > expansion.yml
        ''', test=False),
        OD([('command', 'expansions.update'),
            ('params', OD([
                ('file', 'mongoc/expansion.yml'),
            ]))]),
        shell_exec(r'''
        rm -f *.tar.gz
        curl --retry 5 https://s3.amazonaws.com/mciuploads/${project}/${branch_name}/mongo-c-driver-${CURRENT_VERSION}.tar.gz --output mongoc.tar.gz -sS --max-time 120
        ''', test=False, continue_on_err=True),
    )),
    ('upload release', Function(
        shell_exec(
            r'[ -f mongoc/cmake_build/mongo*gz ] && mv mongoc/cmake_build/mongo*gz mongoc.tar.gz',
            errexit=False, test=False),
        s3_put(
            '${project}/${branch_name}/mongo-c-driver-${CURRENT_VERSION}.tar.gz',
            project_path=False, aws_key='${aws_key}',
            aws_secret='${aws_secret}', local_file='mongoc.tar.gz',
            bucket='mciuploads', permissions='public-read',
            content_type='${content_type|application/x-gzip}'),
    )),
    ('upload build', Function(
        targz_pack('${build_id}.tar.gz', 'mongoc', './**'),
        s3_put('${build_variant}/${revision}/${task_name}/${build_id}.tar.gz',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='${build_id}.tar.gz', bucket='mciuploads',
               permissions='public-read',
               content_type='${content_type|application/x-gzip}'),
    )),
    ('release archive', Function(
        shell_mongoc(r'''
        # Need modern Sphinx for :caption: in literal includes.
        python -m virtualenv venv
        cd venv
        . bin/activate
        ./bin/pip install sphinx
        cd ..

        export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
        export MONGOC_TEST_SKIP_LIVE=on
        export MONGOC_TEST_SKIP_SLOW=on
        sh .evergreen/check-release-archive.sh
        '''),
    )),
    ('install ssl', Function(
        shell_mongoc(r'SSL=${SSL} sh .evergreen/install-ssl.sh', test=False),
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
    ('upload docs', Function(
        shell_exec(r'''
        export AWS_ACCESS_KEY_ID=${aws_key}
        export AWS_SECRET_ACCESS_KEY=${aws_secret}
        aws s3 cp doc/html s3://mciuploads/${project}/docs/libbson/${CURRENT_VERSION} --recursive --acl public-read --region us-east-1
        ''', test=False, silent=True, working_dir='mongoc/cmake_build/src/libbson'),
        s3_put('docs/libbson/${CURRENT_VERSION}/index.html',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongoc/cmake_build/src/libbson/doc/html/index.html',
               bucket='mciuploads', permissions='public-read',
               content_type='text/html', display_name='libbson docs'),
        shell_exec(r'''
        export AWS_ACCESS_KEY_ID=${aws_key}
        export AWS_SECRET_ACCESS_KEY=${aws_secret}
        aws s3 cp doc/html s3://mciuploads/${project}/docs/libmongoc/${CURRENT_VERSION} --recursive --acl public-read --region us-east-1
        ''', test=False, silent=True, working_dir='mongoc/cmake_build/src/libmongoc'),
        s3_put('docs/libmongoc/${CURRENT_VERSION}/index.html',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongoc/cmake_build/src/libmongoc/doc/html/index.html',
               bucket='mciuploads', permissions='public-read',
               content_type='text/html', display_name='libmongoc docs'),
    )),
    ('upload man pages', Function(
        shell_mongoc(r'''
        # Get "aha", the ANSI HTML Adapter.
        git clone --depth 1 https://github.com/theZiz/aha.git aha-repo
        cd aha-repo
        make
        cd ..
        mv aha-repo/aha .

        sh .evergreen/man-pages-to-html.sh libbson cmake_build/src/libbson/doc/man > bson-man-pages.html
        sh .evergreen/man-pages-to-html.sh libmongoc cmake_build/src/libmongoc/doc/man > mongoc-man-pages.html
        ''', test=False, silent=True),
        s3_put('man-pages/libbson/${CURRENT_VERSION}/index.html',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongoc/bson-man-pages.html', bucket='mciuploads',
               permissions='public-read', content_type='text/html',
               display_name='libbson man pages'),
        s3_put('man-pages/libmongoc/${CURRENT_VERSION}/index.html',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_file='mongoc/mongoc-man-pages.html', bucket='mciuploads',
               permissions='public-read', content_type='text/html',
               display_name='libmongoc man pages'),
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
    ('abi report', Function(
        shell_mongoc(r'''
        sh .evergreen/abi-compliance-check.sh
        ''', test=False),
        shell_mongoc(r'''
        export AWS_ACCESS_KEY_ID=${aws_key}
        export AWS_SECRET_ACCESS_KEY=${aws_secret}
        aws s3 cp abi-compliance/compat_reports s3://mciuploads/${project}/%s/abi-compliance/compat_reports --recursive --acl public-read --region us-east-1

        if [ -e ./abi-compliance/abi-error.txt ]; then
          exit 1
        else
          exit 0
        fi
        ''' % (build_path,), silent=True, test=False),
        s3_put(build_path + '/abi-compliance/compat_report.html',
               aws_key='${aws_key}', aws_secret='${aws_secret}',
               local_files_include_filter='mongoc/abi-compliance/compat_reports/**/*.html',
               bucket='mciuploads', permissions='public-read',
               content_type='text/html', display_name='ABI Report:'),
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
    ('backtrace', Function(
        shell_mongoc(r'./.evergreen/debug-core-evergreen.sh', test=False),
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
        sh .evergreen/integration-tests.sh
        ''', test=False),
    )),
    ('run tests', Function(
        shell_mongoc(r'''
        export COMPRESSORS='${COMPRESSORS}'
        export CC='${CC}'
        export AUTH=${AUTH}
        export SSL=${SSL}
        export URI=${URI}
        export IPV4_ONLY=${IPV4_ONLY}
        export VALGRIND=${VALGRIND}
        export MONGOC_TEST_URI=${URI}
        export DNS=${DNS}
        export ASAN=${ASAN}
        export CLIENT_SIDE_ENCRYPTION=${CLIENT_SIDE_ENCRYPTION}
        # Hide credentials.
        set +o xtrace
        set +o errexit
        # Only set creds if testing with Client Side Encryption.
        # libmongoc may build with CSE enabled (if the host has libmongocrypt installed)
        # and will try to run those tests (which fail on ASAN unless spawning is bypassed).
        if [ -n "$CLIENT_SIDE_ENCRYPTION" ]; then
          echo "testing with CSE"
          export MONGOC_TEST_AWS_SECRET_ACCESS_KEY="${client_side_encryption_aws_secret_access_key}"
          export MONGOC_TEST_AWS_ACCESS_KEY_ID="${client_side_encryption_aws_access_key_id}"

          export MONGOC_TEST_AZURE_TENANT_ID="${client_side_encryption_azure_tenant_id}"
          export MONGOC_TEST_AZURE_CLIENT_ID="${client_side_encryption_azure_client_id}"
          export MONGOC_TEST_AZURE_CLIENT_SECRET="${client_side_encryption_azure_client_secret}"

          export MONGOC_TEST_GCP_EMAIL="${client_side_encryption_gcp_email}"
          export MONGOC_TEST_GCP_PRIVATEKEY="${client_side_encryption_gcp_privatekey}"
        fi
        set -o errexit
        sh .evergreen/run-tests.sh
        '''),
    )),
    ('run tests bson', Function(
        shell_mongoc(r'CC="${CC}" sh .evergreen/run-tests-bson.sh'),
    )),
    # Use "silent=True" to hide output since errors may contain credentials.
    ('run auth tests', Function(
        shell_mongoc(r'''
        export AUTH_HOST='${auth_host}'
        export AUTH_PLAIN='${auth_plain}'
        export AUTH_MONGODBCR='${auth_mongodbcr}'
        export AUTH_GSSAPI='${auth_gssapi}'
        export AUTH_CROSSREALM='${auth_crossrealm}'
        export AUTH_GSSAPI_UTF8='${auth_gssapi_utf8}'
        export ATLAS_FREE='${atlas_free}'
        export ATLAS_FREE_SRV='${atlas_free_srv}'
        export ATLAS_REPLSET='${atlas_replset}'
        export ATLAS_REPLSET_SRV='${atlas_replset_srv}'
        export ATLAS_SHARD='${atlas_shard}'
        export ATLAS_SHARD_SRV='${atlas_shard_srv}'
        export ATLAS_TLS11='${atlas_tls11}'
        export ATLAS_TLS11_SRV='${atlas_tls11_srv}'
        export ATLAS_TLS12='${atlas_tls12}'
        export ATLAS_TLS12_SRV='${atlas_tls12_srv}'
        export REQUIRE_TLS12='${require_tls12}'
        export OBSOLETE_TLS='${obsolete_tls}'
        export VALGRIND='${valgrind}'
        sh .evergreen/run-auth-tests.sh
        ''', silent=True),
    )),
    ('run mock server tests', Function(
        shell_mongoc(
            r'CC="${CC}" VALGRIND=${VALGRIND} sh .evergreen/run-mock-server-tests.sh'),
    )),
    ('cleanup', Function(
        shell_mongoc(r'''
        cd MO
        mongo-orchestration stop
        ''', test=False),
    )),
    ('windows fix', Function(
        shell_mongoc(r'''
        for i in $(find .evergreen -name \*.sh); do
          cat $i | tr -d '\r' > $i.new
          mv $i.new $i
        done
        ''', test=False),
    )),
    ('make files executable', Function(
        shell_mongoc(r'''
        for i in $(find .evergreen -name \*.sh); do
          chmod +x $i
        done
        ''', test=False),
    )),
    ('prepare kerberos', Function(
        shell_mongoc(r'''
        if test "${keytab|}"; then
           echo "${keytab}" > /tmp/drivers.keytab.base64
           base64 --decode /tmp/drivers.keytab.base64 > /tmp/drivers.keytab
           cat .evergreen/kerberos.realm | $SUDO tee -a /etc/krb5.conf
        fi
        ''', test=False, silent=True),
    )),
    ('link sample program', Function(
        shell_mongoc(r'''
        # Compile a program that links dynamically or statically to libmongoc,
        # using variables from pkg-config or CMake's find_package command.
        export BUILD_SAMPLE_WITH_CMAKE=${BUILD_SAMPLE_WITH_CMAKE}
        export BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=${BUILD_SAMPLE_WITH_CMAKE_DEPRECATED}
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  sh .evergreen/link-sample-program.sh
        LINK_STATIC=1 sh .evergreen/link-sample-program.sh
        '''),
    )),
    ('link sample program bson', Function(
        shell_mongoc(r'''
        # Compile a program that links dynamically or statically to libbson,
        # using variables from pkg-config or from CMake's find_package command.
        BUILD_SAMPLE_WITH_CMAKE=  BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=  sh .evergreen/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=  BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=1 sh .evergreen/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=  sh .evergreen/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=  LINK_STATIC=1 sh .evergreen/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1 LINK_STATIC=  sh .evergreen/link-sample-program-bson.sh
        BUILD_SAMPLE_WITH_CMAKE=1 BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1 LINK_STATIC=1 sh .evergreen/link-sample-program-bson.sh
        '''),
    )),
    ('link sample program MSVC', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically or statically to it, using variables from CMake's
        # find_package command.
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  cmd.exe /c .\\.evergreen\\link-sample-program-msvc.cmd
        LINK_STATIC=1 cmd.exe /c .\\.evergreen\\link-sample-program-msvc.cmd
        '''),
    )),
    ('link sample program mingw', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically to it, using variables from pkg-config.exe.
        cmd.exe /c .\\.evergreen\\link-sample-program-mingw.cmd
        '''),
    )),
    ('link sample program MSVC bson', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically or statically to it, using variables from CMake's
        # find_package command.
        export ENABLE_SSL=${ENABLE_SSL}
        export ENABLE_SNAPPY=${ENABLE_SNAPPY}
        LINK_STATIC=  cmd.exe /c .\\.evergreen\\link-sample-program-msvc-bson.cmd
        LINK_STATIC=1 cmd.exe /c .\\.evergreen\\link-sample-program-msvc-bson.cmd
        '''),
    )),
    ('link sample program mingw bson', Function(
        shell_mongoc(r'''
        # Build libmongoc with CMake and compile a program that links
        # dynamically to it, using variables from pkg-config.exe.
        cmd.exe /c .\\.evergreen\\link-sample-program-mingw-bson.cmd
        '''),
    )),
    ('update codecov.io', Function(
        shell_mongoc(r'''
        export CODECOV_TOKEN=${codecov_token}
        curl -s https://codecov.io/bash | bash
        ''', test=False),
    )),
    ('debug-compile-coverage-notest-nosasl-nossl', Function(
        shell_mongoc(r'''
        export EXTRA_CONFIGURE_FLAGS="-DENABLE_COVERAGE=ON -DENABLE_EXAMPLES=OFF"
        DEBUG=ON CC='${CC}' MARCH='${MARCH}' SASL=OFF SSL=OFF SKIP_MOCK_TESTS=ON sh .evergreen/compile.sh
        '''),
    )),
    ('debug-compile-coverage-notest-nosasl-openssl', Function(
        shell_mongoc(r'''
        export EXTRA_CONFIGURE_FLAGS="-DENABLE_COVERAGE=ON -DENABLE_EXAMPLES=OFF"
        DEBUG=ON CC='${CC}' MARCH='${MARCH}' SASL=OFF SSL=OPENSSL SKIP_MOCK_TESTS=ON sh .evergreen/compile.sh
        '''),
    )),
    ('build mongohouse', Function(
        shell_mongoc(r'''
        if [ ! -d "drivers-evergreen-tools" ]; then
           git clone git@github.com:mongodb-labs/drivers-evergreen-tools.git
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
        ./src/libmongoc/test-libmongoc --no-fork -l /mongohouse/* -d
        unset RUN_MONGOHOUSE_TESTS

        '''),
    )),
    ('test versioned api', Function(
        shell_mongoc(r'''
        export COMPRESSORS='${COMPRESSORS}'
        export CC='${CC}'
        export AUTH=${AUTH}
        export SSL=${SSL}
        export URI=${URI}
        export IPV4_ONLY=${IPV4_ONLY}
        export VALGRIND=${VALGRIND}
        export MONGOC_TEST_URI=${URI}
        export DNS=${DNS}
        export ASAN=${ASAN}
        export MONGODB_API_VERSION=1
        sh .evergreen/run-tests.sh
        unset MONGODB_API_VERSION

        '''),
    )),
    ('run aws tests', Function(
        shell_mongoc(r'''
        # Add AWS variables to a file.
        # Clone in one directory above so it does not get uploaded in working directory.
        git clone --depth=1 git://github.com/mongodb-labs/drivers-evergreen-tools.git ../drivers-evergreen-tools
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
        set -o errexit
        # Export the variables we need to construct URIs
        set +o xtrace
        export IAM_AUTH_ECS_ACCOUNT=${iam_auth_ecs_account}
        export IAM_AUTH_ECS_SECRET_ACCESS_KEY=${iam_auth_ecs_secret_access_key}
        sh ./.evergreen/run-aws-tests.sh ${TESTCASE}
        ''')
    ))
])

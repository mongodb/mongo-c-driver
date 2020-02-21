# Test runner for AWS authentication.
# 
# This script is meant to be run in parts (so to isolate the AWS tests).
# Pass the desired test first argument (REGULAR, EC2, ECS, ASSUME_ROLE, LAMBDA)
#
# Example:
# run-aws-tests.sh EC2
#
# Optional environment variables:
#
# DRIVERS_TOOLS
#   The path to clone of git://github.com/mongodb-labs/drivers-evergreen-tools.git.
#   Defaults to $(pwd)../drivers-evergreen-tools
# CDRIVER_BUILD
#   The path to the build of mongo-c-driver (e.g. mongo-c-driver/cmake-build).
#   Defaults to $(pwd)
# CDRIVER_ROOT
#   The path to mongo-c-driver source (may be same as CDRIVER_BUILD).
#   Defaults to $(pwd)
# MONGODB_BINARIES
#   The path to mongodb binaries.
#   Defaults to $(pwd)/mongodb/bin
# IAM_AUTH_ECS_ACCOUNT and IAM_AUTH_ECS_SECRET_ACCESS_KEY
#   Set to access key id/secret access key. Required for some tests.

# Do not trace
set +o xtrace

case "$(uname -s | tr '[:upper:]' '[:lower:]')" in
    cygwin*)
        OS="WINDOWS"
        ;;
    darwin)
        OS="MACOS"
        ;;
    *)
        OS="LINUX"
        ;;
esac

# Fail on any command returning a non-zero exit status.
set -o errexit

DRIVERS_TOOLS=${DRIVERS_TOOLS:-$(pwd)/../drivers-evergreen-tools}
CDRIVER_BUILD=${CDRIVER_BUILD:-$(pwd)}
CDRIVER_ROOT=${CDRIVER_ROOT:-$(pwd)}
MONGODB_BINARIES=${MONGODB_BINARIES:-$(pwd)/mongodb/bin}
TESTCASE=$1

echo "DRIVERS_TOOLS=$DRIVERS_TOOLS"
echo "CDRIVER_BUILD=$CDRIVER_BUILD"
echo "MONGODB_BINARIES=$MONGODB_BINARIES"
echo "CDRIVER_ROOT=$CDRIVER_ROOT"
echo "TESTCASE=$TESTCASE"

MONGOC_PING=$CDRIVER_BUILD/src/libmongoc/mongoc-ping
# Add libmongoc-1.0 and libbson-1.0 to library path, so mongoc-ping can find them at runtime.
if [ "$OS" = "WINDOWS" ]; then
    export PATH=$PATH:$CDRIVER_BUILD/src/libmongoc/Debug:$CDRIVER_BUILD/src/libbson/Debug
    chmod +x src/libmongoc/Debug/* src/libbson/Debug/* || true
    MONGOC_PING=$CDRIVER_BUILD/src/libmongoc/Debug/mongoc-ping.exe
elif [ "$OS" = "MACOS" ]; then
    export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$CDRIVER_BUILD/src/libmongoc:$CDRIVER_BUILD/src/libbson
elif [ "$OS" = "LINUX" ]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CDRIVER_BUILD/src/libmongoc:$CDRIVER_BUILD/src/libbson
fi

expect_success () {
    echo "Should succeed:"
    MONGODB_URI=$1
    if ! $MONGOC_PING $MONGODB_URI; then
        echo "Unexpected auth failure"
        exit 1
    fi
}

expect_failure () {
    echo "Should fail:"
    MONGODB_URI=$1
    if $MONGOC_PING $MONGODB_URI >output.txt 2>&1; then
        echo "Unexpected - authed but it should not have"
        exit 1
    else
        echo "auth failed as expected"
    fi

    if ! grep "Authentication failed" output.txt >/dev/null; then
        echo "Unexpected, error was not an authentication failure:"
        cat output.txt
        exit 1
    fi
}

url_encode() {
    encoded=""
    for c in $(echo $1 | grep -o . ); do
        case $c in
            [a-zA-Z0-9.~_-]) encoded=$encoded$c ;;
            *) encoded=$encoded$(printf '%%%02X' "'$c") ;;
        esac
    done
    echo $encoded
}

# Some of the setup scripts expect mongo to be on path.
export PATH=$PATH:$MONGODB_BINARIES

if [ "REGULAR" = "$TESTCASE" ]; then
    echo "===== Testing regular auth via URI ====="

    # Create user on $external db.
    cd $DRIVERS_TOOLS/.evergreen/auth_aws
    mongo --verbose aws_e2e_regular_aws.js
    cd -

    USER_ENCODED=$(url_encode $IAM_AUTH_ECS_ACCOUNT)
    PASS_ENCODED=$(url_encode $IAM_AUTH_ECS_SECRET_ACCESS_KEY)

    expect_success "mongodb://$USER_ENCODED:$PASS_ENCODED@localhost/?authMechanism=MONGODB-AWS"
    expect_failure "mongodb://$USER_ENCODED:bad_password@localhost/?authMechanism=MONGODB-AWS"

    exit 0
fi

if [ "ASSUME_ROLE" = "$TESTCASE" ]; then
    echo "===== Testing auth with session token via URI with AssumeRole ====="
    cd $DRIVERS_TOOLS/.evergreen/auth_aws
    mongo --verbose aws_e2e_assume_role.js
    cd -

    USER=$(jq -r '.AccessKeyId' $DRIVERS_TOOLS/.evergreen/auth_aws/creds.json)
    PASS=$(jq -r '.SecretAccessKey' $DRIVERS_TOOLS/.evergreen/auth_aws/creds.json)
    TOKEN=$(jq -r '.SessionToken' $DRIVERS_TOOLS/.evergreen/auth_aws/creds.json)

    USER_ENCODED=$(url_encode $USER)
    PASS_ENCODED=$(url_encode $PASS)
    TOKEN_ENCODED=$(url_encode $TOKEN)

    expect_success "mongodb://$USER_ENCODED:$PASS_ENCODED@localhost/aws?authMechanism=MONGODB-AWS&authSource=\$external&authMechanismProperties=AWS_SESSION_TOKEN:$TOKEN_ENCODED"
    expect_failure "mongodb://$USER_ENCODED:$PASS_ENCODED@localhost/aws?authMechanism=MONGODB-AWS&authSource=\$external&authMechanismProperties=AWS_SESSION_TOKEN:bad_token"
    exit 0
fi

if [ "LAMBDA" = "$TESTCASE" ]; then
    echo "===== Testing auth via environment variables ====="

    cd $DRIVERS_TOOLS/.evergreen/auth_aws
    mongo --verbose aws_e2e_assume_role.js
    cd -

    USER=$(jq -r '.AccessKeyId' $DRIVERS_TOOLS/.evergreen/auth_aws/creds.json)
    PASS=$(jq -r '.SecretAccessKey' $DRIVERS_TOOLS/.evergreen/auth_aws/creds.json)
    TOKEN=$(jq -r '.SessionToken' $DRIVERS_TOOLS/.evergreen/auth_aws/creds.json)

    echo "Valid credentials - should succeed:"
    export AWS_ACCESS_KEY_ID=$USER
    export AWS_SECRET_ACCESS_KEY=$PASS
    export AWS_SESSION_TOKEN=$TOKEN
    expect_success "mongodb://localhost/?authMechanism=MONGODB-AWS"
    exit 0
fi

if [ "EC2" = "$TESTCASE" ]; then
    echo "===== Testing auth via EC2 task metadata ====="
    # Do necessary setup for EC2
    # Create user on $external db.
    cd $DRIVERS_TOOLS/.evergreen/auth_aws
    mongo --verbose aws_e2e_ec2.js
    cd -
    echo "Valid credentials via EC2 - should succeed"
    expect_success "mongodb://localhost/?authMechanism=MONGODB-AWS"
    exit 0
fi

if [ "ECS" = "$TESTCASE" ]; then
    echo "===== Testing auth via ECS task metadata ====="
    ls $DRIVERS_TOOLS
    # Overwrite the test that gets run by remote ECS task.
    cp $CDRIVER_ROOT/.evergreen/ecs_hosted_test.js $DRIVERS_TOOLS/.evergreen/auth_aws/lib
    chmod 777 $CDRIVER_ROOT/.evergreen/run-mongodb-aws-ecs-test.sh

    cd $DRIVERS_TOOLS/.evergreen/auth_aws

    PROJECT_DIRECTORY=$CDRIVER_ROOT

    cat <<EOF > setup.js
    const mongo_binaries = "$MONGODB_BINARIES";
    const project_dir = "$PROJECT_DIRECTORY";
EOF

    $MONGODB_BINARIES/mongo --nodb setup.js aws_e2e_ecs.js

    cd -
    exit 0
fi

echo "Unexpected testcase '$TESTCASE'"
exit 1
#! /bin/bash

# ECS tests have paths /root/mongoc/

echo "run-mongodb-aws-ecs-test.sh"

CDRIVER_BUILD=/root/mongoc

# Set paths so mongoc-ping can find dependencies
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CDRIVER_BUILD/src/libmongoc:$CDRIVER_BUILD/src/libbson

expect_success () {
    echo "Should succeed:"
    MONGODB_URI=$1
    if ! $CDRIVER_BUILD/src/libmongoc/mongoc-ping $MONGODB_URI; then
        echo "Unexpected auth failure"
        exit 1
    fi
}

expect_success $1
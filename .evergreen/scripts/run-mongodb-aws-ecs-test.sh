#!/usr/bin/env bash

# ECS tests have paths /root/mongoc/

echo "run-mongodb-aws-ecs-test.sh"

# Set paths so mongoc-ping can find dependencies
export LD_LIBRARY_PATH
LD_LIBRARY_PATH+=":/root/mongoc/src/libmongoc"
LD_LIBRARY_PATH+=":/root/mongoc/src/libbson"

expect_success() {
  echo "Should succeed:"
  if ! /root/mongoc/src/libmongoc/mongoc-ping "${1:?}"; then
    echo "Unexpected auth failure" 1>&2
    exit 1
  fi
}

expect_success "${1:?}"

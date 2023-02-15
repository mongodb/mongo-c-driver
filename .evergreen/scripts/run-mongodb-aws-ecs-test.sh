#!/usr/bin/env bash

# ECS tests have paths /root/mongoc/

echo "run-mongodb-aws-ecs-test.sh"


expect_success() {
  echo "Should succeed:"
  if ! /root/mongoc/src/libmongoc/test-awsauth "${1:?}" "EXPECT_SUCCESS"; then
    exit 1
  fi
}

expect_success "${1:?}"

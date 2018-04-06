#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

run_valgrind ()
{
   # Set MONGOC_TEST_VALGRIND to disable asserts that may fail in valgrind
   MONGOC_TEST_VALGRIND=on valgrind \
      --error-exitcode=1 --leak-check=full --gen-suppressions=all \
      --num-callers=32 --suppressions=valgrind.suppressions \
      $@
}

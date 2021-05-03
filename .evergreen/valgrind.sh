#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail

run_valgrind ()
{
   # Set MONGOC_TEST_VALGRIND to disable asserts that may fail in valgrind
   # Do not do leak detection, as ASAN tests are set to detect leaks, and
   # leak detection on valgrind is slower.
   MONGOC_TEST_SKIP_SLOW=on MONGOC_TEST_VALGRIND=on valgrind \
      --error-exitcode=1 --leak-check=no --gen-suppressions=all \
      --keep-stacktraces=none --suppressions=valgrind.suppressions \
      $@
}

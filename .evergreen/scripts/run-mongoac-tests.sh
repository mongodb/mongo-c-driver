#!/usr/bin/env bash

set -o errexit
set -o pipefail

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

# The mongoac test executable is only built when mongoac is enabled: fail loudly rather than reporting a green task
# for a build that did not include it.
check_var_opt ENABLE_MONGOAC
if [[ "${ENABLE_MONGOAC}" != "ON" ]]; then
  echo "ENABLE_MONGOAC must be ON, but is: '${ENABLE_MONGOAC}'" 1>&2
  exit 1
fi

declare test_binary="${mongoc_dir:?}/cmake-build/src/libmongoac/test/test-libmongoac"
if [[ ! -x "${test_binary}" ]]; then
  echo "Test executable not found: ${test_binary}" 1>&2
  exit 1
fi

# Run from the project directory: the JUnit report is written to `mongoac-test-results.xml` relative to it, which is
# where `.evergreen/config_generator/components/funcs/upload_mongoac_test_results.py` expects to find it.
cd "${mongoc_dir:?}"

# The mongoac shared library is produced by cargo, which emits neither a SONAME nor an RPATH on Linux, and the test
# executable is built with `CMAKE_SKIP_RPATH=TRUE`. The build-tree copy of `libmongoac.so` must therefore be on the
# loader path or the executable fails with "cannot open shared object file". Note the *installed* copy is renamed
# `libmongoac0.so`, so it cannot satisfy the executable's `DT_NEEDED` entry: this also ensures the build-tree copy is
# the one under test. `add-build-dirs-to-paths.sh` prepends `EXTRA_LIB_PATH` and covers the remaining dependencies.
export EXTRA_LIB_PATH="${mongoc_dir:?}/cmake-build/src/libmongoac/Debug${EXTRA_LIB_PATH:+:${EXTRA_LIB_PATH}}"
# shellcheck source=.evergreen/scripts/add-build-dirs-to-paths.sh
. "${script_dir:?}/add-build-dirs-to-paths.sh"

ulimit -c unlimited || true

# Catch2 exits non-zero on test failure (and on a malformed invocation), which `errexit` turns into a red task.
"${test_binary:?}" --reporter console --reporter junit::out=mongoac-test-results.xml

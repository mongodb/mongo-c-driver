#!/usr/bin/env bash

set -o errexit
set -o pipefail

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

declare test_binary="${mongoc_dir:?}/cmake-build/src/libmongoac/test/test-libmongoac"
if [[ ! -x "${test_binary}" ]]; then
  echo "Test executable not found: ${test_binary}" 1>&2
  exit 1
fi

# Run from the project directory so the relative `cmake-build` path below resolves. The JUnit report is written to
# the project directory, which is where
# `.evergreen/config_generator/components/funcs/upload_mongoac_test_results.py` expects to find it.
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

# `ctest` is provided by CMake. $PATH does not persist across Evergreen commands, so the CMake installed by the
# compile step is not visible here: install it for this command. (Same pattern as build-mongoac-docs.sh.)
# shellcheck source=.evergreen/scripts/install-build-tools.sh
. "${script_dir:?}/install-build-tools.sh"
install_build_tools

# The tests read `MONGOAC_TEST_URI` (default: mongodb://localhost:27017). Point them at the server that
# `bootstrap-mongo-orchestration` started for this task, whose connection string is published as the
# `MONGODB_URI` expansion.
if [[ -n "${MONGODB_URI:-}" ]]; then
  export MONGOAC_TEST_URI="${MONGODB_URI}"
fi

# Run the suite through CTest, which owns the test registration (catch_discover_tests), so the JUnit report carries
# the same `mongoac/<suite>/<name>` identifiers the Evergreen Tests tab shows. ctest exits non-zero on any failure,
# which `errexit` turns into a red task. `-R '^mongoac/'` restricts the run to this library's tests: the build
# directory also contains libbson/libmongoc tests, which require a live MongoDB server.
ctest --test-dir cmake-build -R '^mongoac/' --no-tests=error --output-on-failure \
  --output-junit "${mongoc_dir:?}/mongoac-test-results.xml"

# CTest's JUnit writer duplicates each test's name in the `classname` attribute, and Evergreen names a test
# `classname.name` when that attribute is non-empty (dots survive; `/` becomes `_`). Set the class to `mongoac` and
# drop the now-redundant `mongoac/` prefix from the names, so the Tests tab shows e.g. `mongoac.runtime_address`.
sed -e '/^[[:space:]]*<testcase /s/ classname="[^"]*"/ classname="mongoac"/' \
  -e '/^[[:space:]]*<testcase /s/ name="mongoac\// name="/' \
  "${mongoc_dir:?}/mongoac-test-results.xml" >|"${mongoc_dir:?}/mongoac-test-results.tmp"
mv "${mongoc_dir:?}/mongoac-test-results.tmp" "${mongoc_dir:?}/mongoac-test-results.xml"

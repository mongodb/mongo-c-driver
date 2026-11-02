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

cd "${mongoc_dir:?}"

export EXTRA_LIB_PATH="${mongoc_dir:?}/cmake-build/src/libmongoac/Debug${EXTRA_LIB_PATH:+:${EXTRA_LIB_PATH}}"
# shellcheck source=.evergreen/scripts/add-build-dirs-to-paths.sh
. "${script_dir:?}/add-build-dirs-to-paths.sh"

ulimit -c unlimited || true

# shellcheck source=.evergreen/scripts/install-build-tools.sh
. "${script_dir:?}/install-build-tools.sh"
install_build_tools

export RUST_BACKTRACE='1'

if [[ -n "${MONGODB_URI:-}" ]]; then
  export MONGOAC_TEST_URI="${MONGODB_URI}"
fi

ctest_flags=(
  --test-dir cmake-build
  -R '^mongoac/'
  --parallel
  --no-tests=error
  --output-on-failure
  --output-junit "${mongoc_dir:?}/mongoac-test-results.xml"
)
catch2_flags=(
  -w InfiniteGenerators
  -w NoAssertions
  -w UnmatchedTestSpec
)
ctest "${ctest_flags[@]:?}" -- "${catch2_flags[@]:?}"

# CTest's JUnit writer duplicates each test's name in the `classname` attribute, and Evergreen names a test
# `classname.name` when that attribute is non-empty (dots survive; `/` becomes `_`). Set the class to `mongoac` and
# drop the now-redundant `mongoac/` prefix from the names, so the Tests tab shows e.g. `mongoac.runtime_address`.
sed -e '/^[[:space:]]*<testcase /s/ classname="[^"]*"/ classname="mongoac"/' \
  -e '/^[[:space:]]*<testcase /s/ name="mongoac\// name="/' \
  "${mongoc_dir:?}/mongoac-test-results.xml" >|"${mongoc_dir:?}/mongoac-test-results.tmp"
mv "${mongoc_dir:?}/mongoac-test-results.tmp" "${mongoc_dir:?}/mongoac-test-results.xml"

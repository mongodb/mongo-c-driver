#!/usr/bin/env bash
#
# clang-format.sh
#
# Usage:
#   ./tools/poetry.sh install --with=dev
#   ./tools/poetry.sh run .evergreen/scripts/clang-format.sh
#   DRYRUN=1 ./tools/poetry.sh run .evergreen/scripts/clang-format.sh
#   ./tools/poetry.sh run env DRYRUN=1 .evergreen/scripts/clang-format.sh
#
# This script is meant to be run from the project root directory.

: "${DRYRUN:-}"

set -o errexit
set -o pipefail

clang-format --version

source_dirs=(
  src/common
  src/libbson
  src/libmongoc
)

mapfile -t source_files < <(find "${source_dirs[@]:?}" -name '*.[ch]')

if [[ -n "${DRYRUN:-}" ]]; then
  clang-format --dry-run -Werror "${source_files[@]:?}"
else
  clang-format --verbose -i "${source_files[@]:?}"
fi

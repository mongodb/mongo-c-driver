#!/usr/bin/env bash
#
# format.sh
#
# Usage:
#   etc/shfmt-format-all.sh
#
# This script is meant to be run from the project root directory.

set -o errexit
set -o pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir:?}/.." && pwd)"

command -v cargo >/dev/null

(cd "${root_dir:?}/src/libmongoac" && cargo fmt)

#!/usr/bin/env bash
# Collect logs from and destroy a deployment started by dsc-start.sh. No-op if none was started.
#
# TODO: move into drivers-evergreen-tools (.evergreen/disagg/) alongside dsc-start.sh.

set -o pipefail

# shellcheck source=.evergreen/scripts/dsc-common.sh
. "$(dirname "${BASH_SOURCE[0]}")/dsc-common.sh"

if [[ ! -f "${TAG_FILE}" ]]; then
  echo "OK - no DSC deployment running"
  exit 0
fi

run_slslocal --compose logs --follow=false >"$(pwd)/sls-logs.txt" 2>&1 || true
gzip -f "$(pwd)/sls-logs.txt" || true # Uploaded by the stop-dsc function.
run_slslocal --compose destroy || true
rm -f "${TAG_FILE}"

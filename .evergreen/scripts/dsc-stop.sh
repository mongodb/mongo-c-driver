#!/usr/bin/env bash
# Collect logs from and destroy a deployment started by dsc-start.sh. No-op if none was started.
#
# TODO: move into drivers-evergreen-tools (.evergreen/disagg/) alongside dsc-start.sh.

set -o pipefail

# shellcheck source=.evergreen/scripts/dsc-common.sh
. "$(dirname "${BASH_SOURCE[0]}")/dsc-common.sh"

# Remove the replica set member names added to /etc/hosts by dsc-start.sh.
sudo sed -i "/${HOSTS_MARKER}\$/d" /etc/hosts || true

# Remove the port 27017 redirect added by dsc-start.sh.
if [[ -f "${STATE_DIR}/dsc-redirect-port" ]]; then
  sudo iptables -t nat -D OUTPUT -d 127.0.0.1 -p tcp --dport 27017 -j REDIRECT --to-ports "$(cat "${STATE_DIR}/dsc-redirect-port")" || true
  rm -f "${STATE_DIR}/dsc-redirect-port"
fi

if [[ ! -f "${TAG_FILE}" ]]; then
  echo "OK - no DSC deployment running"
  exit 0
fi

run_slslocal --compose logs --follow=false >"$(pwd)/sls-logs.txt" 2>&1 || true
gzip -f "$(pwd)/sls-logs.txt" || true # Uploaded by the stop-dsc function.
run_slslocal --compose destroy || true
rm -f "${TAG_FILE}"

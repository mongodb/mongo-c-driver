#!/usr/bin/env bash
# Collect logs from and destroy a deployment started by dsc-start.sh. No-op if none was started.
#
# TODO: move into drivers-evergreen-tools (.evergreen/disagg/) alongside dsc-start.sh.

set -o pipefail

declare work
work="$(pwd)/dsc-work"

if [[ ! -f "${work}/started" ]]; then
  echo "OK - no DSC deployment running"
  exit 0
fi

declare ecr="664315256653.dkr.ecr.us-east-1.amazonaws.com"
declare arch
case "$(uname -m)" in
aarch64 | arm64) arch="arm64" ;;
*) arch="x86-64" ;;
esac

slslocal() {
  docker run --rm --privileged --network=host \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v "${work}:/root" -v "${work}:${work}" -w "${work}" \
    "${ecr}/disagg-storage/slslocal:latest-linux-${arch}" --compose "$@"
}

slslocal logs --follow=false >"${work}/sls-logs.txt" 2>&1 || true
slslocal destroy || true
rm -f "${work}/started"

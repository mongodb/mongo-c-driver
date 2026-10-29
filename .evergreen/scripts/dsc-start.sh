#!/usr/bin/env bash
# Start an SLS deployment + disaggregated storage (DSC) mongods with `slslocal --compose`.
#
# Inputs (environment):
#   DSC_NUM_CELLS          Number of cells (default: 1).
#   DSC_MONGODS_PER_SHARD  Number of mongods per shard; must be <= DSC_NUM_CELLS (default: 1).
#   DSC_SLS_TAG            SLS ECR image tag (default: newest mainline tag; see resolve_sls_tag).
#   DSC_MONGOD_TAG         mock-data-plane image tag, i.e. server commit (default: slslocal's pinned default).
#   AWS_*                  Credentials with ECR pull access (from ec2.assume_role).
#
# Output: dsc-expansion.yml (in the working directory) containing MONGODB_URI.
#
# TODO: move into drivers-evergreen-tools (.evergreen/disagg/) so other drivers can reuse it.

set -o errexit
set -o pipefail
set -o nounset

# shellcheck source=.evergreen/scripts/dsc-common.sh
. "$(dirname "${BASH_SOURCE[0]}")/dsc-common.sh"

[[ "$(uname -m)" == "x86_64" ]] || fail "expected an x86_64 host"
command -v docker >/dev/null 2>&1 || fail "'docker' is not installed on this host"
command -v aws >/dev/null 2>&1 || fail "the 'aws' CLI is not installed on this host"
command -v python3 >/dev/null 2>&1 || fail "'python3' is not installed on this host"
[[ -n "${AWS_ACCESS_KEY_ID:-}" ]] || fail "AWS_ACCESS_KEY_ID is not set (expected ec2.assume_role credentials)"

mkdir -p "${STATE_DIR}"

declare tag="${DSC_SLS_TAG:-}"
[[ -n "${tag}" ]] || tag="$(resolve_sls_tag)"
# Written before launching so dsc-stop.sh knows a deployment may be running.
echo "${tag}" >"${TAG_FILE}"
echo "Using SLS image tag: ${tag}"

docker_login
write_aws_profile

declare image="${ECR_ROOT}/disagg-storage/slslocal:${tag}"
docker pull "${image}" || fail "docker pull of '${image}' failed"

declare -a launch_args=(
  --sls-ecr-tag "${tag}"
  --localstack
  --aws-profile-name default
  --num-cells "${DSC_NUM_CELLS:-1}"
  --mongods-per-shard "${DSC_MONGODS_PER_SHARD:-1}"
)
if [[ -n "${DSC_MONGOD_TAG:-}" ]]; then
  launch_args+=(--mongod-tag "${DSC_MONGOD_TAG}")
fi

# TODO: confirm enableTestCommands=1 is set on mongods by slslocal (required by fail point tests); add a flag if not.
run_slslocal --compose launch "${launch_args[@]}" || fail "slslocal launch failed"
run_slslocal --compose wait --timeout 10m || fail "cluster did not become healthy within 10 minutes"

# The replica set members advertise names (shardN-nodeM-mongod) that only resolve inside the containers, but their
# ports are published on the host. Map the names to loopback so that replica set discovery works from the host.
declare -a member_names
mapfile -t member_names < <(python3 "$(dirname "${BASH_SOURCE[0]}")/dsc-uri.py" "${STATE_DIR}" --member-names)
[[ "${#member_names[@]}" -gt 0 ]] || fail "unable to determine replica set member names"
echo "127.0.0.1 ${member_names[*]} ${HOSTS_MARKER}" | sudo tee -a /etc/hosts >/dev/null ||
  fail "unable to add replica set member names to /etc/hosts"

# Some tests hardcode localhost:27017, :27018 and :27019 (the ports of a default local replica set) instead of using
# the test URI. SLS mongods listen on other published ports, so redirect those local ports to the mongods, in order.
# Best effort: tests that need it will fail if a redirect cannot be installed.
declare -a published_ports
mapfile -t published_ports < <(python3 "$(dirname "${BASH_SOURCE[0]}")/dsc-uri.py" "${STATE_DIR}" --published-ports)
: >"${REDIRECTS_FILE}"
for i in "${!published_ports[@]}"; do
  declare default_port=$((27017 + i))
  sudo iptables -t nat -A OUTPUT -d 127.0.0.1 -p tcp --dport "${default_port}" -j REDIRECT --to-ports "${published_ports[i]}" ||
    echo "WARNING: unable to redirect 127.0.0.1:${default_port} to port ${published_ports[i]}" 1>&2
  echo "${default_port} ${published_ports[i]}" >>"${REDIRECTS_FILE}"
done

# Build the connection string from ports.json, asking one node for the replica set name over the wire protocol.
python3 "$(dirname "${BASH_SOURCE[0]}")/dsc-uri.py" "${STATE_DIR}" >dsc-expansion.yml || fail "unable to determine MONGODB_URI"
cat dsc-expansion.yml

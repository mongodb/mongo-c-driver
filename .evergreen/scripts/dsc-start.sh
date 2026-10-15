#!/usr/bin/env bash
# Start an SLS deployment + disaggregated storage (DSC) mongods with `slslocal --compose`.
#
# Inputs (environment):
#   DSC_NUM_CELLS          Number of cells (default: 1).
#   DSC_MONGODS_PER_SHARD  Number of mongods per shard; must be <= DSC_NUM_CELLS (default: 1).
#   DSC_SLS_TAG            SLS ECR image tag (default: latest).
#   DSC_MONGOD_TAG         mock-data-plane image tag, i.e. server commit (default: latest).
#   AWS_*                  Credentials with ECR pull access (from ec2.assume_role).
#
# Output: dsc-expansion.yml (in the working directory) containing MONGODB_URI.
#
# TODO: move into drivers-evergreen-tools (.evergreen/disagg/) so other drivers can reuse it.

set -o errexit
set -o pipefail

declare ecr="664315256653.dkr.ecr.us-east-1.amazonaws.com"
declare arch
case "$(uname -m)" in
aarch64 | arm64) arch="arm64" ;;
*) arch="x86-64" ;;
esac

declare work
work="$(pwd)/dsc-work"
mkdir -p "${work}"

aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin "${ecr}"

slslocal() {
  docker run --rm --privileged --network=host \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v "${work}:/root" -v "${work}:${work}" -w "${work}" \
    -e AWS_ACCESS_KEY_ID -e AWS_SECRET_ACCESS_KEY -e AWS_SESSION_TOKEN \
    "${ecr}/disagg-storage/slslocal:latest-linux-${arch}" --compose "$@"
}

# Marker so the post-task teardown knows a deployment may be running.
touch "${work}/started"

# TODO: confirm enableTestCommands=1 is set on mongods by slslocal (required by fail point tests); add a flag if not.
slslocal launch \
  --sls-ecr-tag "${DSC_SLS_TAG:-latest}" \
  --mongod-tag "${DSC_MONGOD_TAG:-latest}" \
  --localstack \
  --num-cells "${DSC_NUM_CELLS:-1}" \
  --mongods-per-shard "${DSC_MONGODS_PER_SHARD:-1}" \
  --output-root "${work}"
slslocal wait
slslocal ports >"${work}/ports.json"

declare hosts
hosts="$(jq -r '[.shards_by_log_id[] | .[]] | join(",")' "${work}/ports.json")"

declare setname
setname="$(mongosh --quiet "mongodb://${hosts%%,*}/?directConnection=true" --eval 'db.hello().setName')"

echo "MONGODB_URI: \"mongodb://${hosts}/?replicaSet=${setname}\"" >dsc-expansion.yml
cat dsc-expansion.yml

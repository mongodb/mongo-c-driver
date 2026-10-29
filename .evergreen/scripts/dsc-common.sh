#!/usr/bin/env bash
# Shared helpers for dsc-start.sh and dsc-stop.sh. Sourced; not executable on its own.
#
# Adapted from the DRIVERS-3190 experiment: all cluster components run as containers pulled from the
# SRE-owned ECR registry, so the host only needs docker, the aws CLI and python3.

ECR_ROOT="664315256653.dkr.ecr.us-east-1.amazonaws.com"
STATE_DIR="${HOME}/.slslocal"
AWS_DIR="${STATE_DIR}/aws"
TAG_FILE="${STATE_DIR}/dsc-image-tag"
HOSTS_MARKER="# dsc-evergreen"
REDIRECTS_FILE="${STATE_DIR}/dsc-redirects"

fail() {
  echo "ERROR: $*" 1>&2
  exit 1
}

# SLS images are tagged "linux-x86-64-<10gen/sls git sha>" for mainline builds; there is no "latest" tag.
# The slslocal image and the SLS service images share a tag, so pick the most recently pushed mainline tag
# present in both the slslocal and log repositories. Avoids `slslocal --sls-ecr-tag latest`, which reads
# s3://sls-versions (not permitted for the read-only ECR role).
resolve_sls_tag() {
  local tmpdir
  tmpdir="$(mktemp -d)"
  for repo in slslocal log; do
    aws ecr describe-images --region us-east-1 \
      --repository-name "disagg-storage/${repo}" \
      --filter tagStatus=TAGGED \
      --output json >"${tmpdir}/${repo}.json" ||
      fail "failed to list image tags in disagg-storage/${repo}"
  done
  local status=0
  python3 - "${tmpdir}/slslocal.json" "${tmpdir}/log.json" <<'PY' || status=$?
import json, re, sys


def mainline(path, suffix=""):
    pattern = r"linux-x86-64-[0-9a-f]{40}" + suffix
    with open(path) as f:
        images = json.load(f)["imageDetails"]
    out = {}
    for image in images:
        for tag in image.get("imageTags", []):
            if re.fullmatch(pattern, tag):
                out[tag] = image.get("imagePushedAt", 0)
    return out


def common(suffix=""):
    a = mainline(sys.argv[1], suffix)
    b = mainline(sys.argv[2], suffix)
    return {t: b[t] for t in set(a) & set(b)}


tags = common() or common("-debug")
if not tags:
    sys.exit("ERROR: no common mainline linux-x86-64 image tag in the slslocal and log repositories")
print(max(tags, key=tags.get))
PY
  rm -rf "${tmpdir}"
  [[ "${status}" -eq 0 ]] || fail "failed to resolve an SLS image tag from ECR"
}

docker_login() {
  aws ecr get-login-password --region us-east-1 |
    docker login --username AWS --password-stdin "${ECR_ROOT}" ||
    fail "docker login to '${ECR_ROOT}' failed"
}

# Materialize the assumed-role credentials as a "default" AWS profile for slslocal's container.
write_aws_profile() {
  mkdir -p "${AWS_DIR}"
  (
    umask 077
    cat >"${AWS_DIR}/credentials" <<CREDS
[default]
aws_access_key_id = ${AWS_ACCESS_KEY_ID}
aws_secret_access_key = ${AWS_SECRET_ACCESS_KEY}
aws_session_token = ${AWS_SESSION_TOKEN:-}
CREDS
    printf '[default]\nregion = us-east-1\n' >"${AWS_DIR}/config"
  )
}

# Same mounts as the `just slslocal` recipe in 10gen/sls: the docker socket, and the state directory both as
# the container home and at its host path (compose bind mounts are resolved by the host daemon).
run_slslocal() {
  local tag
  tag="$(cat "${TAG_FILE}")"
  docker run --rm \
    -v /var/run/docker.sock:/var/run/docker.sock \
    --privileged \
    --network=host \
    -v "${STATE_DIR}:/root" \
    -v "${STATE_DIR}:${STATE_DIR}" \
    -v "${AWS_DIR}:/root/.aws" \
    -e SLSLOCAL_HOST_HOME="${HOME}" \
    -e AWS_ACCESS_KEY_ID \
    -e AWS_SECRET_ACCESS_KEY \
    -e AWS_SESSION_TOKEN \
    -e AWS_REGION=us-east-1 \
    -e AWS_DEFAULT_REGION=us-east-1 \
    "${ECR_ROOT}/disagg-storage/slslocal:${tag}" "$@"
}

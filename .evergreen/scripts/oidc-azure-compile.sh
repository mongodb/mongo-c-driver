#!/usr/bin/env bash
set -o errexit
set -o pipefail
set -o nounset

# shellcheck source=.evergreen/scripts/use-tools.sh
. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths # Sets MONGOC_DIR

cd "$MONGOC_DIR"

if [[ "${distro_id:?}" == "debian11-small" ]]; then
  # Temporary workaround for lack of uv on `debian11`. TODO: remove after DEVPROD-23011 is resolved.
  uv_dir="$(mktemp -d)"
  python3 -m virtualenv "${uv_dir:?}"
  # shellcheck source=/dev/null
  (. "${uv_dir:?}/bin/activate" && python -m pip install uv)
  PATH="${uv_dir:?}/bin:${PATH:-}"
  command -V uv >/dev/null
fi

. .evergreen/scripts/install-build-tools.sh
install_build_tools
export CMAKE_GENERATOR="Ninja"

# Use ccache if able.
. .evergreen/scripts/find-ccache.sh
find_ccache_and_export_vars "$(pwd)" || true

echo "Compile test-libmongoc ... begin"
# Disable unnecessary dependencies. test-libmongoc is copied to a remote host for testing, which may not have all dependent libraries.
cmake \
  -DENABLE_SASL=OFF \
  -DENABLE_SNAPPY=OFF \
  -DENABLE_ZSTD=OFF \
  -DENABLE_ZLIB=OFF \
  -DENABLE_SRV=OFF \
  -DENABLE_CLIENT_SIDE_ENCRYPTION=OFF \
  -DENABLE_EXAMPLES=OFF \
  -DENABLE_SRV=OFF \
  -S. -Bcmake-build
cmake --build cmake-build --target test-libmongoc --parallel
echo "Compile test-libmongoc ... end"

# Create tarball for remote testing.
echo "Creating test-libmongoc tarball ... begin"

# Copy test binary and JSON test files. All JSON test files are needed to start test-libmongoc.
tar -czf test-libmongoc.tar.gz \
  .evergreen/scripts/oidc-azure-test.sh \
  ./cmake-build/src/libmongoc/test-libmongoc \
  src/libmongoc/tests/json \
  src/libbson/tests/json
echo "Creating test-libmongoc tarball ... end"

cat <<EOT > oidc-remote-test-expansion.yml
OIDC_TEST_TARBALL: ${MONGOC_DIR}/test-libmongoc.tar.gz
EOT

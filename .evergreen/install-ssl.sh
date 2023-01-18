#!/usr/bin/env bash

set -o errexit
set -o pipefail

to_absolute() (
  cd "${1:?}" && pwd
)

print_var() {
  printf "%s: %s\n" "${1:?}" "${!1:-}"
}

check_var_opt() {
  printf -v "${1:?}" "%s" "${!1:-"${2:-}"}"
  print_var "${1}"
}

check_var_opt "SSL" "no"

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/..")"

declare install_dir="${mongoc_dir}/install-dir"

declare -a ssl_extra_flags

build_target_if_exists() {
  if make -n "${1:?}" 2>/dev/null; then
    make -s "${@}"
  fi
}

install_openssl() {
  declare ssl_version="${SSL##openssl-}"
  declare tmp
  tmp="$(echo "${ssl_version:?}" | tr . _)"
  curl -L --retry 5 -o ssl.tar.gz "https://github.com/openssl/openssl/archive/OpenSSL_${tmp}.tar.gz"
  tar zxf ssl.tar.gz
  pushd "openssl-OpenSSL_${tmp}"
  (
    set -o xtrace
    ./config --prefix="${install_dir}" "${ssl_extra_flags[@]}" shared -fPIC
    make -j depend
    build_target_if_exists "build_crypto"       # <1.1.0
    build_target_if_exists "build_engines" "-j" # <1.1.0
    build_target_if_exists "build_ssl" "-j"     # <1.1.0
    build_target_if_exists "build_libs" "-j"    # <1.1.0
    make -j
    make install_sw
  ) >/dev/null
  popd # "openssl-OpenSSL_${tmp}"
}

install_openssl_fips() {
  curl --retry 5 -o fips.tar.gz https://www.openssl.org/source/openssl-fips-2.0.16.tar.gz
  tar zxf fips.tar.gz
  pushd openssl-fips-2.0.16
  (
    set -x xtrace
    ./config --prefix="${install_dir}" -fPIC
    make -j build_crypto
    make build_fips
    make install_sw
  ) >/dev/null
  popd # openssl-fips-2.0.16
  ssl_extra_flags=("--openssldir=${install_dir}" --with-fipsdir="${install_dir}" "fips")
  SSL="${SSL%-fips}"
  install_openssl
}

install_libressl() {
  curl --retry 5 -o ssl.tar.gz "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/${SSL}.tar.gz"
  tar zxf ssl.tar.gz
  pushd "${SSL}"
  (
    set -o xtrace
    ./configure --prefix="${install_dir}"
    make -s -j install
  ) >/dev/null
  popd # "${SSL}"
}

case "${SSL}" in
openssl-*-fips)
  export LC_ALL
  LC_ALL="C" # Silence perl locale warnings.
  install_openssl_fips
  ;;

openssl-*)
  export LC_ALL
  LC_ALL="C" # Silence perl locale warnings.
  install_openssl
  ;;

libressl-*)
  install_libressl
  ;;
esac

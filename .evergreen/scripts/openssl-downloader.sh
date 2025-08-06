#!/usr/bin/env bash

# shellcheck disable=SC2034

openssl_version_to_url() {
  declare version
  version="${1:?"usage: openssl_version_to_url <version>"}"

  command -v perl >/dev/null || return

  declare uversion # 1.2.3 -> 1_2_3
  uversion="$(echo "${version:?}" | perl -lpe 's|\.|_|g')" || return

  declare download_url
  if [[ "${version:?}" == 1.0.2 ]]; then
    url="https://github.com/openssl/openssl/releases/download/OpenSSL_${uversion:?}u/openssl-${version:?}u.tar.gz"
  elif [[ "${version:?}" == 1.1.1 ]]; then
    url="https://github.com/openssl/openssl/releases/download/OpenSSL_${uversion:?}w/openssl-${version:?}w.tar.gz"
  else
    url="https://github.com/openssl/openssl/releases/download/openssl-${version:?}/openssl-${version:?}.tar.gz"
  fi

  echo "${url:?}"
}

# Download the requested OpenSSL version as `openssl.tar.gz`.
openssl_download() {
  declare version
  version="${1:?"usage: openssl_download_checksum <version>"}"

  command -v curl perl sha256sum >/dev/null || return

  declare url
  url="$(openssl_version_to_url "${version:?}")" || return

  declare openssl_checksum_1_0_2="ecd0c6ffb493dd06707d38b14bb4d8c2288bb7033735606569d8f90f89669d16"
  declare openssl_checksum_1_1_1="cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8"
  declare openssl_checksum_3_0_9="eb1ab04781474360f77c318ab89d8c5a03abc38e63d65a603cabbf1b00a1dc90" # FIPS 140-2
  declare openssl_checksum_3_0_17="dfdd77e4ea1b57ff3a6dbde6b0bdc3f31db5ac99e7fdd4eaf9e1fbb6ec2db8ce"
  declare openssl_checksum_3_1_2="a0ce69b8b97ea6a35b96875235aa453b966ba3cba8af2de23657d8b6767d6539" # FIPS 140-3
  declare openssl_checksum_3_1_8="d319da6aecde3aa6f426b44bbf997406d95275c5c59ab6f6ef53caaa079f456f"
  declare openssl_checksum_3_2_5="b36347d024a0f5bd09fefcd6af7a58bb30946080eb8ce8f7be78562190d09879"
  declare openssl_checksum_3_3_4="8d1a5fc323d3fd351dc05458457fd48f78652d2a498e1d70ffea07b4d0eb3fa8"
  declare openssl_checksum_3_4_2="17b02459fc28be415470cccaae7434f3496cac1306b86b52c83886580e82834c"
  declare openssl_checksum_3_5_1="529043b15cffa5f36077a4d0af83f3de399807181d607441d734196d889b641f"

  declare checksum_name
  checksum_name="openssl_checksum_$(echo "${version:?}" | perl -lpe 's|\.|_|g')" || return

  [[ -n "$(eval "echo \${${checksum_name:?}}")" ]] || {
    echo "missing checksum for OpenSSL version \"${version:?}\""
    return 1
  } >&2

  declare filename
  filename="openssl.tar.gz"

  echo "Downloading OpenSSL ${version:?}..."
  curl -sSL -o "${filename:?}" "${url:?}" || return
  echo "Downloading OpenSSL ${version:?}... done."

  echo "${!checksum_name:?} ${filename:?}" | sha256sum -c >/dev/null || return
}

# Download the OpenSSL FIPS Object Module 2.0: https://wiki.openssl.org/index.php/FIPS_module_2.0
openssl_download_fips() {
  command -v curl perl sha256sum >/dev/null || return

  declare version uversion
  version="2.0.16"
  uversion="$(echo "${version:?}" | perl -lpe 's|\.|_|g')" || return

  declare checksum
  checksum="19cf79cc43517c82609954133d05ed879fe12c4cab5441bb634f96b8e8d0e0c4"

  declare filename
  filename="openssl-fips.tar.gz"

  echo "Downloading OpenSSL FIPS Module ${version:?}..."
  curl -sSL -o openssl-fips.tar.gz "https://github.com/openssl/openssl/releases/download/OpenSSL-fips-${uversion:?}/openssl-fips-ecp-${version:?}.tar.gz"
  echo "Downloading OpenSSL FIPS Module ${version:?}... done."

  echo "${checksum:?} ${filename:?}" | sha256sum -c >/dev/null || return
}

#!/usr/bin/env bash

# A convenient helper function to download the list of checksums for the specified release.
# The output of this function should be copy-pasted as-is into the array of checksums below.
download_checksums() {
  declare version
  version="${1:?"usage: download_checkums <version>"}"

  for checksum in $(curl -sSL "https://github.com/astral-sh/uv/releases/download/${version:?}/dist-manifest.json" | jq -r '.releases[0].artifacts.[] | select(startswith("uv-") and (endswith(".zip.sha256") or endswith(".tar.gz.sha256")))'); do
    curl -sSL "https://github.com/astral-sh/uv/releases/download/${version:?}/${checksum:?}"
  done
}

# Patches the specified uv-installer.sh script with checksums.
patch_uv_installer() {
    declare script version
    script="${1:?"usage: patch_uv_installer <path/to/uv-installer.sh> <version>"}"
    version="${2:?"usage: patch_uv_installer <path/to/uv-installer.sh> <version>"}"

    [[ -f "${script:?}" ]] || {
        echo "${script:?} does not exist?"
        return 1
    } >&2

    command -v perl >/dev/null || return

    # Ensure the uv-installer.sh script's version matches the expected version.
    app_version="$(perl -lne 'print $1 if m|APP_VERSION="([^"]+)"|' "${script:?}")" || return

    [[ "${app_version:?}" == "${version:?}" ]] || {
        echo "${script:?} version ${app_version:?} does not match expected version ${version:?}"
        return 1
    } >&2

    # The output of the `download_checksums` helper function.
    checksums=(
        c3eddc0e314abb8588f1cdf312f0b060d79e1906eff8f43b64a05ff5e2727872  uv-aarch64-apple-darwin.tar.gz
        5b3a80d385d26fb9f63579a0712d020ec413ada38a6900e88fdfd41b58795b7e *uv-aarch64-pc-windows-msvc.zip
        3bb77b764618f65a969da063d1c4a507d8de5360ca2858f771cab109fa879a4d  uv-aarch64-unknown-linux-gnu.tar.gz
        40ba6e62de35820e8460eacee2b5b8f4add70a834d3859f7a60cdfc6b19ab599  uv-aarch64-unknown-linux-musl.tar.gz
        f108a49a17b0700d7121b0215575f96c46a203774ed80ef40544005d7af74a67  uv-arm-unknown-linux-musleabihf.tar.gz
        730d8ef57f221ecc572d47b227ecbd8261be08157efb351311f7bc1f6c1c944a  uv-armv7-unknown-linux-gnueabihf.tar.gz
        b78dacab7c2fb352301d8997c0c705c3959a4e44d7b3afe670aee2397a2c9ab3  uv-armv7-unknown-linux-musleabihf.tar.gz
        08482edef8b077e12e73f76e6b4bb0300c054b8009cfac5cc354297f47d24623 *uv-i686-pc-windows-msvc.zip
        0ce384911d4af9007576ceba2557c5d474a953ced34602ee4e09bd888cee13c0  uv-i686-unknown-linux-gnu.tar.gz
        b6462dc8190c7a1eafa74287d8ff213764baa49e098aeeb522fa479d29e1c0bf  uv-i686-unknown-linux-musl.tar.gz
        9a8e8a8927df9fa39af79214ab1acfc227dba9d9e690a424cef1dc17296161a8  uv-powerpc64-unknown-linux-gnu.tar.gz
        4880a8e2ba5086e7ed4bd3aecfdae5e353da569ddaac02cd3db598b4c8e77193  uv-powerpc64le-unknown-linux-gnu.tar.gz
        0cd68055cedbc5b1194e7e7ab2b35ac7aa1d835c586fb3778c7acb0e8a8ac822  uv-riscv64gc-unknown-linux-gnu.tar.gz
        8cc2e70bee35c9e437c2308f130b79acc0d7c43e710296990ed76e702e220912  uv-s390x-unknown-linux-gnu.tar.gz
        b799253441726351bc60c2e91254a821001e5e2e22a0e2b077d8983f583e8139  uv-x86_64-apple-darwin.tar.gz
        60870fa18d438737088e533ed06617549e42531c522cc9a8fe4455d8e745dc29 *uv-x86_64-pc-windows-msvc.zip
        8ca3db7b2a3199171cfc0870be1f819cb853ddcec29a5fa28dae30278922b7ba  uv-x86_64-unknown-linux-gnu.tar.gz
        38ade73396b48fce89d9d1cb8a7e8f02b6e18a2d87467525ee8fb7e09899f70d  uv-x86_64-unknown-linux-musl.tar.gz
    )

    # Substitution:
    #     local _checksum_value
    # ->
    #     local _checksum_value="sha256"
    perl -i'' -lpe "s|local _checksum_style$|local _checksum_style=\"sha256\"|" "${script:?}" || return

    # Substitution (for each checksum + artifact in the checksums array):
    #     case "$_artifact_name" in
    #         ...
    #         "<artifact>")
    #         ...
    #     esac
    # ->
    #     case "$_artifact_name" in
    #         ...
    #         "<artifact>") _checksum_value="<checksum>"
    #         ...
    #     esac
    for ((i=0; i<"${#checksums[@]}"; i+=2)); do
        declare checksum artifact
        checksum="${checksums[i]:?}"
        artifact="${checksums[i+1]:?}"

        [[ "${artifact:?}" =~ ^\* ]] && artifact="${artifact:1}"
        perl -i'' -lpe "s|(\"${artifact:?}\"\))|\$1 _checksum_value=\"${checksum:?}\"|" "${script:?}" || return
    done
}

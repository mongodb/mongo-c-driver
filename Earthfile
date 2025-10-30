VERSION --arg-scope-and-set --pass-args --use-function-keyword 0.7

# For target names, descriptions, and build parameters, run the "doc" Earthly subcommand.
# Example use: <earthly> +build --from=ubuntu:22.04 --sasl=off --tls=OpenSSL --c_compiler=gcc

# Allow setting the "default" container image registry to use for image short names (e.g. to Amazon ECR).
ARG --global default_search_registry=docker.io

# Set a base container image at the root so that this project can be imported
FROM $default_search_registry/alpine:3.20

# Not intended to be overridden, but provide some defaults used across targets
ARG --global __source_dir = "/opt/mcd/source"
ARG --global __build_dir  = "/opt/mcd/build"

init:
    ARG --required from
    FROM --pass-args $from
    DO +INIT

# build-environment :
#   A target that just presents the environment required for a mongo-c-driver build
build-environment:
    FROM --pass-args +init
    DO --pass-args +INSTALL_DEPS

# configure :
#   Installs deps and configures the project without building.
configure:
    FROM --pass-args +build-environment

    # Various important paths for the build
    ARG source_dir = $__source_dir
    ARG build_dir  = $__build_dir

    # Add the source tree into the container
    DO +COPY_SOURCE --into=$source_dir

    # Configure the project
    ARG --required tls
    ARG --required sasl
    RUN cmake -S "$source_dir" -B "$build_dir" -G "Ninja Multi-Config" \
        -D ENABLE_MAINTAINER_FLAGS=ON \
        -D ENABLE_SHM_COUNTERS=ON \
        -D ENABLE_SASL=$(echo $sasl | __str upper) \
        -D ENABLE_SNAPPY=AUTO \
        -D ENABLE_SRV=ON \
        -D ENABLE_ZLIB=BUNDLED \
        -D ENABLE_SSL=$(echo $tls | __str upper) \
        -D ENABLE_COVERAGE=OFF \
        -D ENABLE_DEBUG_ASSERTIONS=ON \
        -Werror

# build :
#   Install deps, configures, and builds libmongoc and libbson.
#
#   Building this target requires certain arguments to be specified, the most important
#   being `--from`, which specifies the base container image that is used for the build
build:
    ARG config          = "RelWithDebInfo"
    LET install_prefix  = "/opt/mongo-c-driver"

    # Do the configure step. Force the default value for $build_dir
    FROM --pass-args +configure --build_dir=$__build_dir

    # Run the add-ccache command here. This needs to run directly within the same
    # target that makes use of it, to ensure that the CACHE line has an effect
    # within that target
    DO --pass-args +ADD_CCACHE

    # Build the project
    RUN cmake --build $__build_dir --config $config
    # Install to the local prefix
    RUN cmake --install $__build_dir --prefix="$install_prefix" --config $config
    # Export the build results and the install tree
    SAVE ARTIFACT $__build_dir/* /build-tree/
    SAVE ARTIFACT $install_prefix/* /root/

# test-example will build one of the libmongoc example projects using the build
# that comes from the +build target. Arguments for +build should also be provided
test-example:
    FROM --pass-args +build-environment
    # Grab the built library
    COPY --pass-args +build/root /opt/mongo-c-driver
    # Add the example files
    COPY --dir \
        src/libmongoc/examples/cmake \
        src/libmongoc/examples/hello_mongoc.c \
        /opt/mongoc-test/
    # Configure and build it
    RUN cmake \
            -S /opt/mongoc-test/cmake/find_package \
            -B /bld \
            -G Ninja \
            -D CMAKE_PREFIX_PATH=/opt/mongo-c-driver
    RUN cmake --build /bld

# test-cxx-driver :
#   Clone and build the mongo-cxx-driver project, using the current mongo-c-driver
#   for the build.
#
# The “--test_mongocxx_ref” argument must be a clone-able Git ref. The driver source
# will be cloned at this point and built.
#
# The “--cxx_version_current” argument will be inserted into the VERSION_CURRENT
# file for the cxx-driver build. The default value is “0.0.0”
#
# Arguments for +build should be provided.
test-cxx-driver:
    FROM --pass-args +build-environment

    # Copy over the C driver libary build
    COPY --pass-args +build/root /opt/mongo-c-driver

    LET source=/opt/mongo-cxx-driver/src
    LET build=/opt/mongo-cxx-driver/bld
    GIT CLONE --branch=$test_mongocxx_ref https://github.com/mongodb/mongo-cxx-driver.git $source

    # Set the VERSION_CURRENT file
    ARG cxx_version_current=0.0.0
    RUN echo $cxx_version_current > $source/build/VERSION_CURRENT

    # Configure the project
    RUN cmake -S $source -B $build -G Ninja -D CMAKE_PREFIX_PATH=/opt/mongo-c-driver -D CMAKE_CXX_STANDARD=17
    # Build
    RUN cmake --build $build

# release-archive :
#   Create a release archive of the source tree. (Refer to dev docs)
release-archive:
    FROM $default_search_registry/alpine:3.20
    RUN apk add git bash
    ARG --required prefix
    ARG --required ref

    WORKDIR /s
    COPY --dir .git .

    # Get the commit hash that we are archiving. Use ^{commit} to "dereference" tag objects
    LET revision = $(git rev-parse "$ref^{commit}")
    RUN git restore --quiet --source=$revision -- VERSION_CURRENT
    LET version = $(cat VERSION_CURRENT)

    # Pick the waterfall project based on the tag
    DO +INIT
    IF __str test "$version" -matches ".*\.0\$"
        # This is a minor release. Link to the build on the main project.
        LET base = "mongo_c_driver"
    ELSE
        # This is (probably) a patch release. Link to the build on the release branch.
        LET base = "mongo_c_driver_latest_release"
    END

    # The augmented SBOM must be manually obtained from a recent execution of
    # the `sbom` task in an Evergreen patch or commit build in advance.
    COPY etc/augmented-sbom.json cyclonedx.sbom.json

    # The full link to the build for this commit
    LET waterfall_url = "https://spruce.mongodb.com/version/${base}_${revision}"
    # Insert the URL into the SSDLC report
    COPY etc/ssdlc.md ssdlc_compliance_report.md
    RUN sed -i "
        s|@waterfall_url@|$waterfall_url|g
        s|@version@|$version|g
    " ssdlc_compliance_report.md
    # Generate the archive
    RUN git archive -o release.tar.gz \
        --prefix="$prefix/" \ # Set the archive path prefix
        "$revision" \ # Add the source tree
        --add-file cyclonedx.sbom.json \ # Add the SBOM
        --add-file ssdlc_compliance_report.md
    SAVE ARTIFACT release.tar.gz

# Obtain the signing public key. Exported as an artifact /c-driver.pub
signing-pubkey:
    FROM $default_search_registry/alpine:3.20
    RUN apk add curl
    RUN curl --location --silent --fail "https://pgp.mongodb.com/c-driver.pub" -o /c-driver.pub
    SAVE ARTIFACT /c-driver.pub

# sign-file :
#   Sign an arbitrary file. This uses internal MongoDB tools and requires authentication
#   to be used to access them. (Refer to dev docs)
sign-file:
    # Pull from Garasign:
    FROM 901841024863.dkr.ecr.us-east-1.amazonaws.com/release-infrastructure/garasign-gpg
    # Copy the file to be signed
    ARG --required file
    COPY $file /s/file
    # Run the GPG signing command. Requires secrets!
    RUN --secret GRS_CONFIG_USER1_USERNAME --secret GRS_CONFIG_USER1_PASSWORD \
        gpgloader && \
        gpg --yes --verbose --armor --detach-sign --output=/s/signature.asc /s/file
    # Export the detatched signature
    SAVE ARTIFACT /s/signature.asc /
    # Verify the file signature against the public key
    COPY +signing-pubkey/c-driver.pub /s/
    RUN touch /keyring && \
        gpg --no-default-keyring --keyring /keyring --import /s/c-driver.pub && \
        gpgv --keyring=/keyring /s/signature.asc /s/file

# signed-release :
#   Generate a signed release artifact. Refer to the "Earthly" page of our dev docs for more information.
#   (Refer to dev docs)
signed-release:
    FROM $default_search_registry/alpine:3.20
    RUN apk add git
    # The version of the release. This affects the filepaths of the output and is the default for --ref
    ARG --required version
    # The Git revision of the repository to be archived. By default, archives the tag of the given version
    ARG ref=refs/tags/$version
    # File stem and archive prefix:
    LET stem="mongo-c-driver-$version"
    WORKDIR /s
    # Run the commands "locally" so that the files can be transferred between the
    # targets via the host filesystem.
    LOCALLY
    # Clean out a scratch space for us to work with
    LET rel_dir = ".scratch/release"
    RUN rm -rf -- "$rel_dir"
    # Primary artifact files
    LET rel_tgz = "$rel_dir/$stem.tar.gz"
    LET rel_asc = "$rel_dir/$stem.tar.gz.asc"
    # Make the release archive:
    COPY (+release-archive/ --prefix=$stem --ref=$ref) $rel_dir/
    RUN mv $rel_dir/release.tar.gz $rel_tgz
    # Sign the release archive:
    COPY (+sign-file/signature.asc --file $rel_tgz) $rel_asc
    # Save them as an artifact.
    SAVE ARTIFACT $rel_dir /dist
    # Remove our scratch space from the host. Getting at the artifacts requires `earthly --artifact`
    RUN rm -rf -- "$rel_dir"

# This target is simply an environment in which the SilkBomb executable is available.
silkbomb:
    FROM 901841024863.dkr.ecr.us-east-1.amazonaws.com/release-infrastructure/silkbomb:2.0
    # Alias the silkbomb executable to a simpler name:
    RUN ln -s /python/src/sbom/silkbomb/bin /usr/local/bin/silkbomb

# sbom-generate :
#   Generate/update the etc/cyclonedx.sbom.json file from the etc/purls.txt file.
#
# This target will update the existing etc/cyclonedx.sbom.json file in-place based
# on the content of etc/purls.txt and etc/cyclonedx.sbom.json.
sbom-generate:
    FROM +silkbomb
    # Copy in the relevant files:
    WORKDIR /s
    COPY etc/purls.txt etc/cyclonedx.sbom.json /s/
    # Update the SBOM file:
    RUN silkbomb update \
        --refresh \
        --no-update-sbom-version \
        --purls purls.txt \
        --sbom-in cyclonedx.sbom.json \
        --sbom-out cyclonedx.sbom.json
    # Save the result back to the host:
    SAVE ARTIFACT /s/cyclonedx.sbom.json AS LOCAL etc/cyclonedx.sbom.json

# sbom-generate-new-serial-number:
#   Equivalent to +sbom-generate but includes the --generate-new-serial-number
#   flag to generate a new unique serial number and reset the SBOM version to 1.
#
# This target will update the existing etc/cyclonedx.sbom.json file in-place based
# on the content of etc/purls.txt and etc/cyclonedx.sbom.json.
sbom-generate-new-serial-number:
    FROM +silkbomb
    # Copy in the relevant files:
    WORKDIR /s
    COPY etc/purls.txt etc/cyclonedx.sbom.json /s/
    # Update the SBOM file:
    RUN silkbomb update \
        --refresh \
        --generate-new-serial-number \
        --purls purls.txt \
        --sbom-in cyclonedx.sbom.json \
        --sbom-out cyclonedx.sbom.json
    # Save the result back to the host:
    SAVE ARTIFACT /s/cyclonedx.sbom.json AS LOCAL etc/cyclonedx.sbom.json

# sbom-validate:
#   Validate the SBOM Lite for the given branch.
sbom-validate:
    FROM +silkbomb
    # Copy in the relevant files:
    WORKDIR /s
    COPY etc/purls.txt etc/cyclonedx.sbom.json /s/
    # Run the SilkBomb tool to download the artifact that matches the requested branch
    RUN silkbomb validate \
            --purls purls.txt \
            --sbom-in cyclonedx.sbom.json \
            --exclude jira

snyk:
    FROM --platform=linux/amd64 $default_search_registry/ubuntu:24.04
    RUN apt-get update && apt-get -y install curl
    RUN curl --location https://github.com/snyk/cli/releases/download/v1.1291.1/snyk-linux -o /usr/local/bin/snyk
    RUN chmod a+x /usr/local/bin/snyk

snyk-test:
    FROM +snyk
    WORKDIR /s
    # Take the scan from within the `src/` directory. This seems to help Snyk
    # actually find the external dependencies that live there.
    COPY --dir src .
    WORKDIR src/
    # Snaptshot the repository and run the scan
    RUN --no-cache --secret SNYK_TOKEN \
        snyk test --unmanaged --json > snyk.json
    SAVE ARTIFACT snyk.json

# snyk-monitor-snapshot :
#   Post a crafted snapshot of the repository to Snyk for monitoring. Refer to "Snyk Scanning"
#   in the dev docs for more details.
snyk-monitor-snapshot:
    FROM +snyk
    WORKDIR /s
    ARG remote="https://github.com/mongodb/mongo-c-driver.git"
    ARG --required branch
    ARG --required name
    IF test "$remote" = "local"
        COPY --dir src .
    ELSE
        GIT CLONE --branch $branch $remote clone
        RUN mv clone/src .
    END
    # Take the scan from within the `src/` directory. This seems to help Snyk
    # actually find the external dependencies that live there.
    WORKDIR src/
    # Snaptshot the repository and run the scan
    RUN --no-cache --secret SNYK_TOKEN --secret SNYK_ORGANIZATION \
        snyk monitor \
            --org=$SNYK_ORGANIZATION \
            --target-reference=$name \
            --unmanaged \
            --print-deps \
            --project-name=mongo-c-driver \
            --remote-repo-url=https://github.com/mongodb/mongo-c-driver

# test-vcpkg-classic :
#   Builds src/libmongoc/examples/cmake/vcpkg by using vcpkg to download and
#   install a mongo-c-driver build in "classic mode". *Does not* use the local
#   mongo-c-driver repository.
test-vcpkg-classic:
    FROM +vcpkg-base
    RUN vcpkg install mongo-c-driver
    RUN rm -rf _build && \
        make test-classic

# test-vcpkg-manifest-mode :
#   Builds src/libmongoc/examples/cmake/vcpkg by using vcpkg to download and
#   install a mongo-c-driver package based on the content of a vcpkg.json manifest
#   that is injected into the project.
test-vcpkg-manifest-mode:
    FROM +vcpkg-base
    RUN apk add jq
    RUN jq -n ' { \
            name: "test-app", \
            version: "1.2.3", \
            dependencies: ["mongo-c-driver"], \
        }' > vcpkg.json && \
        cat vcpkg.json
    RUN rm -rf _build && \
        make test-manifest-mode

vcpkg-base:
    FROM $default_search_registry/alpine:3.18
    RUN apk add cmake curl gcc g++ musl-dev ninja-is-really-ninja zip unzip tar \
                build-base git pkgconf perl bash linux-headers
    ENV VCPKG_ROOT=/opt/vcpkg-git
    ENV VCPKG_FORCE_SYSTEM_BINARIES=1
    GIT CLONE --branch=2023.06.20 https://github.com/microsoft/vcpkg $VCPKG_ROOT
    RUN $VCPKG_ROOT/bootstrap-vcpkg.sh -disableMetrics && \
        install -spD -m 755 $VCPKG_ROOT/vcpkg /usr/local/bin/
    LET src_dir=/opt/mongoc-vcpkg-example
    COPY src/libmongoc/examples/cmake/vcpkg/ $src_dir
    WORKDIR $src_dir

# verify-headers :
#   Execute CMake header verification on the sources
#
#   See `earthly.rst` for more details.
verify-headers:
    # We test against multiple different platforms, because glibc/musl versions may
    # rearrange their header contents and requirements, so we want to check against as
    # many as possible. Also so set some reasonable build dep so the caller doesn't
    # need to specify. In the future, it is possible that we will need to test
    # other environments and build settings.
    BUILD +do-verify-headers-impl \
        --from alpine:3.19 \
        --from almalinux:8 \
        --from ubuntu:20.04 \
        --from quay.io/centos/centos:stream10 \
        --sasl=off --tls=off --cxx_compiler=gcc --c_compiler=gcc --snappy=off

do-verify-headers-impl:
    FROM --pass-args +configure --build_dir=$__build_dir
    # The "all_verify_interface_header_sets" target is created automatically
    # by CMake for the VERIFY_INTERFACE_HEADER_SETS target property.
    RUN cmake --build $build_dir --target all_verify_interface_header_sets

# run :
#   Run one or more targets simultaneously.
#
# The “--targets” argument should be a single-string space-separated list of
# target names (not including a leading ‘+’) identifying targets to mark for
# execution. Targets will be executed concurrently. Other build arguments
# will be forwarded to the executed targets.
run:
    LOCALLY
    ARG --required targets
    FOR __target IN $targets
        BUILD +$__target
    END

# COPY_SOURCE :
#   Copy source files required for the build into the specified "--into" directory
COPY_SOURCE:
    FUNCTION
    ARG --required into
    COPY --dir \
        build/ \
        CMakeLists.txt \
        COPYING \
        etc/ \
        NEWS \
        README.rst \
        src/ \
        THIRD_PARTY_NOTICES \
        VERSION_CURRENT \
        "$into"

INIT:
    FUNCTION
    IF ! __have_command __have_command
        COPY --chmod=755 tools/__tool /usr/local/bin/__tool
        RUN __tool __init
        IF __can_install epel-release
            RUN __do "Enabling EPEL repositories..." __install epel-release
        END
        IF ! __have_command curl
            RUN __do "Installing curl..." __install curl
        END

        ARG uv_version = "0.8.15"
        ARG uv_install_sh_url = "https://astral.sh/uv/$uv_version/install.sh"
        IF ! __have_command uv
            RUN curl -LsSf "$uv_install_sh_url" \
                    | env UV_UNMANAGED_INSTALL=/opt/uv sh - \
                && __alias uv  /opt/uv/uv \
                && __alias uvx /opt/uv/uvx
        END
    END

# d8888b. d88888b d8888b. d88888b d8b   db d8888b. d88888b d8b   db  .o88b. db    db
# 88  `8D 88'     88  `8D 88'     888o  88 88  `8D 88'     888o  88 d8P  Y8 `8b  d8'
# 88   88 88ooooo 88oodD' 88ooooo 88V8o 88 88   88 88ooooo 88V8o 88 8P       `8bd8'
# 88   88 88~~~~~ 88~~~   88~~~~~ 88 V8o88 88   88 88~~~~~ 88 V8o88 8b         88
# 88  .8D 88.     88      88.     88  V888 88  .8D 88.     88  V888 Y8b  d8    88
# Y8888D' Y88888P 88      Y88888P VP   V8P Y8888D' Y88888P VP   V8P  `Y88P'    YP
#
#
# d888888b d8b   db .d8888. d888888b  .d8b.  db      db      .d8888.
#   `88'   888o  88 88'  YP `~~88~~' d8' `8b 88      88      88'  YP
#    88    88V8o 88 `8bo.      88    88ooo88 88      88      `8bo.
#    88    88 V8o88   `Y8b.    88    88~~~88 88      88        `Y8b.
#   .88.   88  V888 db   8D    88    88   88 88booo. 88booo. db   8D
# Y888888P VP   V8P `8888Y'    YP    YP   YP Y88888P Y88888P `8888Y'

INSTALL_DEPS:
    FUNCTION
    DO +INIT

    IF __can_install build-essential
        RUN __install build-essential
    END
    IF test -f /etc/alpine-release
        RUN __install pkgconfig musl-dev
    END

    RUN __do "Warming up uv caches..." uv --quiet run --with=cmake --with=ninja true && \
        __alias cmake uvx              --with=ninja cmake && \
        __alias ctest uvx --from=cmake --with=ninja ctest && \
        __alias cpack uvx --from=cmake --with=ninja cpack

    # Compilers
    DO --pass-args +ADD_C_COMPILER
    DO --pass-args +ADD_CXX_COMPILER
    # Dev utilities
    DO --pass-args +ADD_CCACHE
    DO --pass-args +ADD_LLD
    # Third-party libraries
    DO --pass-args +ADD_SASL
    DO --pass-args +ADD_SNAPPY
    DO --pass-args +ADD_TLS

ADD_CCACHE:
    FUNCTION
    ARG ccache = on
    IF __bool "$ccache"
        IF __have_command ccache || __can_install ccache
            RUN __have_command ccache || __install ccache
            ENV CCACHE_DIR = /opt/ccache/cache
            CACHE /opt/ccache/cache
            ENV CMAKE_C_COMPILER_LAUNCHER = ccache
            ENV CMAKE_CXX_COMPILER_LAUNCHER = ccache
        END
    END

ADD_LLD:
    FUNCTION
    ARG lld = on
    IF __bool "$lld"
        IF __can_install lld
            RUN __do "Installing LLD linker..." __install lld
        ELSE
            RUN __fail "Do not know how to install LLD on this environment. Set --lld=false or update the ADD_LLD function."
        END
    END

ADD_C_COMPILER:
    FUNCTION
    ARG --required c_compiler
    IF test "$c_compiler" = "gcc"
        IF __can_install gcc
            RUN __install gcc
        ELSE
            RUN __fail "Unable to infer the GCC C compiler for this environment. Update ADD_C_COMPILER!"
        END
        ENV CC=gcc
    ELSE IF test "$c_compiler" = "clang"
        IF __can_install clang
            RUN __install clang
        ELSE
            RUN __fail "Unable to infer the Clang C compiler package for this environment. Update ADD_CXX_COMPILER!"
        END
        IF __can_install compiler-rt
            RUN __install compiler-rt
        END
        ENV CC=clang
    ELSE
        RUN __fail "Unknown C compiler specifier: “%s” (Expected one of “gcc” or “clang”)" "$c_compiler"
    END

ADD_CXX_COMPILER:
    FUNCTION
    ARG --required cxx_compiler
    IF test "$cxx_compiler" = "gcc"
        IF __have_command g++
            # We already have a GCC C++ compiler installed. Nothing needs to be done.
        ELSE IF __can_install gcc-g++
            RUN __install gcc-g++
        ELSE IF __can_install g++
            RUN __install g++
        ELSE IF __can_install gcc-c++
            RUN __install gcc-c++
        ELSE
            RUN __fail "Unable to infer the GCC C++ compiler package for this environment. Update ADD_CXX_COMPILER!"
        END
        ENV CXX=g++
    ELSE IF test "$cxx_compiler" = "clang"
        IF __have_command clang++
            # We already have Clang's C++ compiler available. Nothing to do.
        ELSE IF __can_install clang++
            RUN __install clang++
        ELSE IF __can_install clang
            RUN __install clang
        ELSE
            RUN __fail "Unable to infer the Clang C++ compiler package for this environment. Update ADD_CXX_COMPILER!"
        END
        IF __can_install compiler-rt
            RUN __install compiler-rt
        END
        ENV CXX=clang++
    ELSE IF __str test "$cxx_compiler" -ieq "none"
        # Do not install a C++ compiler
    ELSE
        RUN __fail "Unknown C++ compiler specifier: “%s” (Expected one of “gcc”, “clang”, or “none”)" "$cxx_compiler"
    END

ADD_SNAPPY:
    FUNCTION
    ARG --required snappy
    IF __bool "$snappy"
        IF __can_install snappy-dev
            RUN __install snappy-dev snappy
        ELSE IF __can_install libsnappy-dev
            RUN __install libsnappy-dev
        ELSE IF __can_install snappy-devel
            RUN __install snappy-devel
        END
    END

ADD_TLS:
    FUNCTION
    ARG --required tls
    IF __str test "$tls" -ieq OpenSSL
        # Alpine
        IF __can_install openssl-dev
            RUN __install openssl openssl-dev
        # Debian-based:
        ELSE IF __can_install libssl-dev
            # APT will handle this as a regex to match a libssl runtime package:
            RUN __install libssl-dev
        # RHEL-based
        ELSE IF __can_install openssl-devel
            RUN __install openssl-libs openssl-devel
        # ArchLinux:
        ELSE IF test -f /etc/arch-release
            RUN __install openssl
        # Otherwise, we don't recognize this system:
        ELSE
            RUN __fail "Cannot infer the OpenSSL TLS library package names. Please update the ADD_TLS utility"
        END
    ELSE IF __str test "$tls" -ieq "off"
        # Nothing to do
    ELSE
        RUN __fail "Unknown --tls value “%s” (Expect one of “OpenSSL” or “off”)" "$tls"
    END

ADD_SASL:
    FUNCTION
    ARG --required sasl
    IF __str test "$sasl" -ieq Cyrus
        # Debian-based
        IF __can_install libsasl2-dev
            RUN __install libsasl2-2 libsasl2-dev
        # Alpine:
        ELSE IF __can_install cyrus-sasl-dev
            RUN __install cyrus-sasl cyrus-sasl-dev
        # RHEL-based:
        ELSE IF __can_install cyrus-sasl-devel
            RUN __install cyrus-sasl-lib cyrus-sasl-devel
        # Archlinux
        ELSE IF __can_install libsasl
            RUN __install libsasl
        # Otherwise, error:
        ELSE
            RUN __fail "Cannot infer the Cyrus SASL library package names. Please update the ADD_SASL utility"
        END
    ELSE IF __str test "$sasl" -ieq off
        # Do nothing
    ELSE
        RUN __fail "Unknown value for --sasl “%s” (Expect one of “Cyrus” or “off”)" "$sasl"
    END

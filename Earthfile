VERSION --arg-scope-and-set --pass-args 0.7
LOCALLY

IMPORT ./tools/ AS tools

# For target names, descriptions, and build parameters, run the "doc" Earthly subcommand.
# Example use: <earthly> +build --env=u22 --sasl=off --tls=OpenSSL --c_compiler=gcc

# build :
#   Build libmongoc and libbson using the specified environment.
#
# The --env argument specifies the build environment among the `+env.xyz` environment
# targets, using --purpose=build for the build environment. Refer to the target
# list for a list of available build environments.
build:
    # env is an argument
    ARG --required env
    FROM --pass-args +env.$env --purpose=build
    # The configuration to be built
    ARG config=RelWithDebInfo
    # The prefix at which to install the built result
    ARG install_prefix=/opt/mongo-c-driver
    # Build configuration parameters. Will be case-normalized for CMake usage.
    ARG --required sasl
    ARG --required tls
    LET source_dir=/opt/mongoc/source
    LET build_dir=/opt/mongoc/build
    COPY --dir \
        src/ \
        build/ \
        COPYING \
        CMakeLists.txt \
        README.rst \
        THIRD_PARTY_NOTICES \
        NEWS \
        "$source_dir"
    COPY +version-current/ $source_dir
    ENV CCACHE_HOME=/root/.cache/ccache
    RUN cmake -S "$source_dir" -B "$build_dir" -G "Ninja Multi-Config" \
        -D ENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
        -D ENABLE_MAINTAINER_FLAGS=ON \
        -D ENABLE_SHM_COUNTERS=ON \
        -D ENABLE_EXTRA_ALIGNMENT=OFF \
        -D ENABLE_SASL=$(echo $sasl | __str upper) \
        -D ENABLE_SNAPPY=ON \
        -D ENABLE_SRV=ON \
        -D ENABLE_ZLIB=BUNDLED \
        -D ENABLE_SSL=$(echo $tls | __str upper) \
        -D ENABLE_COVERAGE=ON \
        -D ENABLE_DEBUG_ASSERTIONS=ON \
        -Werror
    RUN --mount=type=cache,target=$CCACHE_HOME \
        env CCACHE_BASE="$source_dir" \
            cmake --build $build_dir --config $config
    RUN cmake --install $build_dir --prefix="$install_prefix" --config $config
    SAVE ARTIFACT /opt/mongoc/build/* /build-tree/
    SAVE ARTIFACT /opt/mongo-c-driver/* /root/

# test-example will build one of the libmongoc example projects using the build
# that comes from the +build target. Arguments for +build should also be provided
test-example:
    ARG --required env
    FROM --pass-args +env.$env --purpose=build
    # Grab the built library
    COPY --pass-args +build/root /opt/mongo-c-driver
    # Add the example files
    COPY --dir \
        src/libmongoc/examples/cmake \
        src/libmongoc/examples/cmake-deprecated \
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
    ARG --required env
    ARG --required test_mongocxx_ref
    FROM --pass-args +env.$env --purpose=build
    ARG cxx_compiler
    IF test "$cxx_compiler" = ""
        # No cxx_compiler is set, so infer based on a possible c_compiler option
        ARG c_compiler
        IF test "$c_compiler" != ""
            # ADD_CXX_COMPILER will remap the C compiler name to an appropriate C++ name
            LET cxx_compiler="$c_compiler"
        ELSE
            LET cxx_compiler =  gcc
        END
    END
    ARG cxx_version_current=0.0.0
    DO tools+ADD_CXX_COMPILER --cxx_compiler=$cxx_compiler
    COPY --pass-args +build/root /opt/mongo-c-driver
    LET source=/opt/mongo-cxx-driver/src
    LET build=/opt/mongo-cxx-driver/bld
    GIT CLONE --branch=$test_mongocxx_ref https://github.com/mongodb/mongo-cxx-driver.git $source
    RUN echo $cxx_version_current > $source/build/VERSION_CURRENT
    RUN cmake -S $source -B $build -G Ninja -D CMAKE_PREFIX_PATH=/opt/mongo-c-driver -D CMAKE_CXX_STANDARD=17
    ENV CCACHE_HOME=/root/.cache/ccache
    ENV CCACHE_BASE=$source
    RUN --mount=type=cache,target=$CCACHE_HOME cmake --build $build

# version-current :
#   Create the VERSION_CURRENT file using Git. This file is exported as an artifact at /
version-current:
    # Run on Alpine, which does this work the fastest
    FROM alpine:3.18
    # Install Python and Git, the only things required for this job:
    RUN apk add git python3
    # Copy only the .git/ directory and calc_release_version, which are enough to get the VERSION_CURRENT
    COPY --dir .git/ build/calc_release_version.py /s/
    # Calculate it:
    RUN cd /s/ && \
        python calc_release_version.py --next-minor > VERSION_CURRENT
    SAVE ARTIFACT /s/VERSION_CURRENT

# PREP_CMAKE "warms up" the CMake installation cache for the current environment
PREP_CMAKE:
    COMMAND
    LET scratch=/opt/mongoc-cmake
    # Copy the minimal amount that we need, as to avoid cache invalidation
    COPY tools/use.sh tools/platform.sh tools/paths.sh tools/base.sh tools/download.sh \
        $scratch/tools/
    COPY .evergreen/scripts/find-cmake-version.sh \
        .evergreen/scripts/use-tools.sh \
        .evergreen/scripts/find-cmake-latest.sh \
        .evergreen/scripts/cmake.sh \
        $scratch/.evergreen/scripts/
    # "Install" a shim that runs our managed CMake executable:
    RUN __alias cmake /opt/mongoc-cmake/.evergreen/scripts/cmake.sh
    # Executing any CMake command will warm the cache:
    RUN cmake --version

env-warmup:
    ARG --required env
    BUILD --pass-args +env.$env --purpose=build
    BUILD --pass-args +env.$env --purpose=test

# Simultaneously builds and tests multiple different platforms
multibuild:
    BUILD +run --targets "test-example" \
        --env=alpine3.16 --env=alpine3.17 --env=alpine3.18 --env=alpine3.19 \
        --env=u20 --env=u22 \
        --env=archlinux \
        --tls=OpenSSL --tls=off \
        --sasl=Cyrus --sasl=off \
        --c_compiler=gcc --c_compiler=clang \
        --test_mongocxx_ref=master
    # Note: At time of writing, Ubuntu does not support LibreSSL, so run those
    #   tests on a separate BUILD line that does not include Ubuntu:
    BUILD +run --targets "test-example" \
        --env=alpine3.16 --env=alpine3.17 --env=alpine3.18 --env=alpine3.19 \
        --env=archlinux \
        --tls=LibreSSL \
        --sasl=Cyrus --sasl=off \
        --c_compiler=gcc --c_compiler=clang \
        --test_mongocxx_ref=master

# release-archive :
#   Create a release archive of the source tree. (Refer to dev docs)
release-archive:
    FROM alpine:3.20
    RUN apk add git bash
    ARG --required sbom_branch
    ARG --required prefix
    ARG --required ref

    WORKDIR /s
    COPY --dir .git .

    # Get the commit hash that we are archiving. Use ^{commit} to "dereference" tag objects
    LET revision = $(git rev-parse "$ref^{commit}")
    RUN git restore --quiet --source=$revision -- VERSION_CURRENT
    LET version = $(cat VERSION_CURRENT)

    # Pick the waterfall project based on the tag
    COPY tools+tools-dir/__str /usr/local/bin/__str
    IF __str test "$version" -matches ".*\.0\$"
        # This is a minor release. Link to the build on the main project.
        LET base = "mongo_c_driver"
    ELSE
        # This is (probably) a patch release. Link to the build on the release branch.
        LET base = "mongo_c_driver_latest_release"
    END

    COPY (+sbom-download/augmented-sbom.json --branch=$sbom_branch) cyclonedx.sbom.json

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
    FROM alpine:3.20
    RUN apk add curl
    RUN curl --location --silent --fail "https://pgp.mongodb.com/c-driver.pub" -o /c-driver.pub
    SAVE ARTIFACT /c-driver.pub

# sign-file :
#   Sign an arbitrary file. This uses internal MongoDB tools and requires authentication
#   to be used to access them. (Refer to dev docs)
sign-file:
    # Pull from Garasign:
    FROM artifactory.corp.mongodb.com/release-tools-container-registry-local/garasign-gpg
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
    FROM alpine:3.20
    RUN apk add git
    # We need to know which branch to get the SBOM from
    ARG --required sbom_branch
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
    COPY (+release-archive/ --branch=$sbom_branch --prefix=$stem --ref=$ref) $rel_dir/
    RUN mv $rel_dir/release.tar.gz $rel_tgz
    # Sign the release archive:
    COPY (+sign-file/signature.asc --file $rel_tgz) $rel_asc
    # Save them as an artifact.
    SAVE ARTIFACT $rel_dir /dist
    # Remove our scratch space from the host. Getting at the artifacts requires `earthly --artifact`
    RUN rm -rf -- "$rel_dir"

# This target is simply an environment in which the SilkBomb executable is available.
silkbomb:
    FROM artifactory.corp.mongodb.com/release-tools-container-registry-public-local/silkbomb:1.0
    # Alias the silkbom executable to a simpler name:
    RUN ln -s /python/src/sbom/silkbomb/bin /usr/local/bin/silkbomb

# sbom-generate :
#   Generate/update the etc/cyclonedx.sbom.json file from the etc/purls.txt file.
#
# This target will update the existing etc/cyclonedx.sbom.json file in-place based
# on the content of etc/purls.txt.
sbom-generate:
    FROM +silkbomb
    # Copy in the relevant files:
    WORKDIR /s
    COPY etc/purls.txt etc/cyclonedx.sbom.json /s/
    # Update the SBOM file:
    RUN silkbomb update \
        --purls purls.txt \
        --sbom-in cyclonedx.sbom.json \
        --sbom-out cyclonedx.sbom.json
    # Save the result back to the host:
    SAVE ARTIFACT /s/cyclonedx.sbom.json AS LOCAL etc/cyclonedx.sbom.json

# sbom-download :
#   Download an augmented SBOM from the Silk server for the given branch. Exports
#   the artifact as /augmented-sbom.json
#
# Requires credentials for silk access.
sbom-download:
    FROM alpine:3.20
    ARG --required branch
    # Run the SilkBomb tool to download the artifact that matches the requested branch
    FROM +silkbomb
    # Set --no-cache, because the remote artifact could change arbitrarily over time
    RUN --no-cache \
        --secret SILK_CLIENT_ID \
        --secret SILK_CLIENT_SECRET \
        silkbomb download \
            --sbom-out augmented-sbom.json \
            --silk-asset-group mongo-c-driver-${branch}
    # Export as /augmented-sbom.json
    SAVE ARTIFACT augmented-sbom.json

# create-silk-asset-group :
#   Create an asset group in Silk for the Git branch if one is not already defined.
#
# Requires credentials for Silk access.
#
# If --branch is not specified, it will be inferred from the current Git branch
create-silk-asset-group:
    ARG branch
    # Get a default value for $branch
    FROM alpine:3.19
    IF test "${branch}" = ""
        LOCALLY
        LET branch=$(git rev-parse --abbrev-ref HEAD)
        RUN --no-cache echo "Inferred asset-group name from Git HEAD to be “${branch}”"
    END
    # Reset to alpine from the LOCALLY above
    FROM alpine:3.19
    RUN apk add python3
    # Copy in the script
    COPY tools/create-silk-asset-group.py /opt/
    # # Run the creation script. Refer to tools/create-silk-asset-group.py for details
    RUN --no-cache \
        --secret SILK_CLIENT_ID \
        --secret SILK_CLIENT_SECRET \
        python /opt/create-silk-asset-group.py \
            --branch=${branch} \
            --project=mongo-c-driver \
            --code-repo-url=https://github.com/mongodb/mongo-c-driver \
            --sbom-lite-path=etc/cyclonedx.sbom.json \
            --exist-ok


snyk:
    FROM --platform=linux/amd64 ubuntu:24.04
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
    FROM alpine:3.18
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


# d88888b d8b   db db    db d888888b d8888b.  .d88b.  d8b   db .88b  d88. d88888b d8b   db d888888b .d8888.
# 88'     888o  88 88    88   `88'   88  `8D .8P  Y8. 888o  88 88'YbdP`88 88'     888o  88 `~~88~~' 88'  YP
# 88ooooo 88V8o 88 Y8    8P    88    88oobY' 88    88 88V8o 88 88  88  88 88ooooo 88V8o 88    88    `8bo.
# 88~~~~~ 88 V8o88 `8b  d8'    88    88`8b   88    88 88 V8o88 88  88  88 88~~~~~ 88 V8o88    88      `Y8b.
# 88.     88  V888  `8bd8'    .88.   88 `88. `8b  d8' 88  V888 88  88  88 88.     88  V888    88    db   8D
# Y88888P VP   V8P    YP    Y888888P 88   YD  `Y88P'  VP   V8P YP  YP  YP Y88888P VP   V8P    YP    `8888Y'

env.u18:
    DO --pass-args +UBUNTU_ENV --version=18.04

env.u20:
    DO --pass-args +UBUNTU_ENV --version=20.04

env.u22:
    DO --pass-args +UBUNTU_ENV --version=22.04

env.alpine3.16:
    DO --pass-args +ALPINE_ENV --version=3.16

env.alpine3.17:
    DO --pass-args +ALPINE_ENV --version=3.17

env.alpine3.18:
    DO --pass-args +ALPINE_ENV --version=3.18

env.alpine3.19:
    DO --pass-args +ALPINE_ENV --version=3.19

env.archlinux:
    FROM --pass-args tools+init-env --from archlinux
    RUN pacman-key --init
    ARG --required purpose

    RUN __install ninja snappy

    IF test "$purpose" = build
        RUN __install ccache
    END

    # We don't install SASL here, because it's pre-installed on Arch
    DO --pass-args tools+ADD_TLS
    DO --pass-args tools+ADD_C_COMPILER
    DO +PREP_CMAKE

ALPINE_ENV:
    COMMAND
    ARG --required version
    FROM --pass-args tools+init-env --from alpine:$version
    # XXX: On Alpine, we just use the system's CMake. At time of writing, it is
    # very up-to-date and much faster than building our own from source (since
    # Kitware does not (yet) provide libmuslc builds of CMake)
    RUN __install bash curl cmake ninja musl-dev make
    ARG --required purpose

    IF test "$purpose" = "build"
        RUN __install snappy-dev ccache
    ELSE IF test "$purpose" = "test"
        RUN __install snappy
    END

    DO --pass-args tools+ADD_SASL
    DO --pass-args tools+ADD_TLS
    # Add "gcc" when installing Clang, since it pulls in a lot of runtime libraries and
    # utils that are needed for linking with Clang
    DO --pass-args tools+ADD_C_COMPILER --clang_pkg="gcc clang compiler-rt"

UBUNTU_ENV:
    COMMAND
    ARG --required version
    FROM --pass-args tools+init-env --from ubuntu:$version
    RUN __install curl build-essential
    ARG --required purpose

    IF test "$purpose" = build
        RUN __install ninja-build gcc ccache libsnappy-dev zlib1g-dev
    ELSE IF test "$purpose" = test
        RUN __install libsnappy1v5 ninja-build
    END

    DO --pass-args tools+ADD_SASL
    DO --pass-args tools+ADD_TLS
    DO --pass-args tools+ADD_C_COMPILER
    DO +PREP_CMAKE

VERSION --arg-scope-and-set --pass-args 0.7
LOCALLY

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
    RUN printf '#!/bin/sh\n /opt/mongoc-cmake/.evergreen/scripts/cmake.sh "$@"\n' \
            > /usr/local/bin/cmake && \
        chmod +x /usr/local/bin/cmake
    # Executing any CMake command will warm the cache:
    RUN cmake --version

# version-current creates the VERSION_CURRENT file using Git. This file is exported as an artifact at /
version-current:
    # Run on Alpine, which does this work the fastest
    FROM alpine:3.18
    # Install Python and Git, the only things required for this job:
    RUN apk add git python3
    COPY --dir .git/ build/calc_release_version.py /s/
    # Calculate it:
    RUN cd /s/ && \
        python calc_release_version.py > VERSION_CURRENT
    SAVE ARTIFACT /s/VERSION_CURRENT

# BUILD_AND_INSTALL executes the mongo-c-driver build and installs it to a prefix
BUILD_AND_INSTALL:
    COMMAND
    ARG config=RelWithDebInfo
    ARG install_prefix=/opt/mongo-c-driver
    ARG enable_sasl=CYRUS
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
        -D ENABLE_SASL=$enable_sasl \
        -D ENABLE_SNAPPY=ON \
        -D ENABLE_SRV=ON \
        -D ENABLE_ZLIB=BUNDLED \
        -D ENABLE_SSL=OPENSSL \
        -D ENABLE_COVERAGE=ON \
        -D ENABLE_DEBUG_ASSERTIONS=ON \
        -Werror
    RUN --mount=type=cache,target=$CCACHE_HOME \
        env CCACHE_BASE="$source_dir" \
            cmake --build $build_dir --config $config
    RUN cmake --install $build_dir --prefix="$install_prefix" --config $config

alpine-base:
    ARG --required version
    FROM alpine:$version
    RUN apk add cmake ninja-is-really-ninja gcc musl-dev

alpine3.18-build-env:
    FROM +alpine-base --version=3.18
    RUN apk add openssl-dev cyrus-sasl-dev snappy-dev ccache

alpine3.18-test-env:
    FROM +alpine-base --version=3.18
    RUN apk add libsasl snappy pkgconfig

archlinux-base:
    FROM archlinux
    RUN pacman --sync --refresh --sysupgrade --noconfirm --quiet ninja gcc snappy

archlinux-build-env:
    FROM +archlinux-base
    DO +PREP_CMAKE

archlinux-test-env:
    FROM +archlinux-base
    DO +PREP_CMAKE

ubuntu-base:
    ARG --required version
    FROM ubuntu:$version
    RUN apt-get update && apt-get -y install curl build-essential

u22-build-env:
    FROM +ubuntu-base --version=22.04
    # Build dependencies:
    RUN apt-get update && apt-get -y install \
            ninja-build gcc ccache libssl-dev libsnappy-dev zlib1g-dev \
            libsasl2-dev
    DO +PREP_CMAKE

u22-test-env:
    FROM +ubuntu-base --version=22.04
    RUN apt-get update && apt-get -y install libsnappy1v5 libsasl2-2 ninja-build
    DO +PREP_CMAKE

# build will build libmongoc and libbson using the specified environment.
#
# The --env argument specifies the build environment, which must be one of:
#   • u22 (Ubuntu 22.04)
#   • archlinux
#   • alpine3.18
build:
    # env is an argument
    ARG --required env
    FROM +$env-build-env
    DO --pass-args +BUILD_AND_INSTALL
    SAVE ARTIFACT /opt/mongoc/build/* /build-tree/
    SAVE ARTIFACT /opt/mongo-c-driver/* /root/

# test-example will build one of the libmongoc example projects using the build
# that comes from the +build target.
test-example:
    ARG --required env
    FROM +$env-test-env
    # Grab the built
    COPY --pass-args +build/root /opt/mongo-c-driver
    COPY --dir \
        src/libmongoc/examples/cmake \
        src/libmongoc/examples/cmake-deprecated \
        src/libmongoc/examples/hello_mongoc.c \
        /opt/mongoc-test/
    RUN cmake \
            -S /opt/mongoc-test/cmake/find_package \
            -B /bld \
            -G Ninja \
            -D CMAKE_PREFIX_PATH=/opt/mongo-c-driver
    RUN cmake --build /bld

# Simultaneously builds and tests multiple different platforms
multibuild:
    BUILD --pass-args +test-example --env=u22 --env=archlinux --env=alpine3.18

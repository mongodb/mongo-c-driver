VERSION 0.6
FROM docker.io/library/alpine:latest

IMPORT ./build/envs AS envs

GIT_CONFIG:
    # Replace all Git SSH URLs with an HTTPS URL. Earthly supports SSH-passthru,
    # but it is simpler to just use HTTPS for public repos that don't require SSH
    COMMAND
    RUN git config --global url.https://github.com/.insteadOf git@github.com:

GET_CMAKE:
    # Download and install CMake
    COMMAND
    # The default version of 3.11.2 is the same as downloaded by find-cmake.sh
    ARG version=3.11.2
    RUN curl -Ls "https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}-Linux-x86_64.sh" \
        | bash /dev/stdin --prefix=/usr/local --exclude-subdir

SETUP:
    COMMAND
    IF __is_debian_based
        RUN __install \
            build-essential pkg-config libssl-dev clang ccache python3-pip jq \
            libsnappy-dev "libsnmp*"  git jq
        IF __is_ubuntu && test 1 = $(( $__env_major_version > 14 ))
            RUN __install python3-virtualenv virtualenv libzstd-dev
        END
    ELSE IF __is_redhat_based
        RUN __install gcc gcc-c++ openssl-devel pkgconfig curl git make
        IF test 1 = $(( $__env_major_version >= 8 ))
            RUN __install snappy libzstd-devel python3-virtualenv
        END
        IF test 1 = $(( $__env_major_version >= 7 ))
            RUN __install jq clang python3 python3-pip
        END
    ELSE IF __is_amazonlinux
        RUN __install \
            gcc gcc-c++ python3 python3-pip openssl-devel clang git make snappy \
            libzstd-devel pkgconfig jq
    ELSE IF __is_archlinux
        RUN __install \
            gcc cmake make python3 openssl pkg-config curl git net-snmp jq
    ELSE IF __is_suse
        RUN __install \
            gcc gcc-c++ make python3 libopenssl-devel pkg-config git \
            net-snmp-devel jq
    END
    DO +GIT_CONFIG
    DO +GET_CMAKE

ubuntu-env:
    # Generate an Ubuntu environment of the given version
    ARG --required version
    FROM envs+ubuntu --version=$version
    DO +SETUP

u14-env:
    # Ubuntu 14.04
    FROM +ubuntu-env --version=14.04

u16-env:
    # Ubuntu 16.04
    FROM +ubuntu-env --version=16.04

u18-env:
    # Ubuntu 18.04
    FROM +ubuntu-env --version=18.04

u20-env:
    # Ubuntu 20.04
    FROM +ubuntu-env --version=20.04

u22-env:
    # Ubuntu 22.04
    FROM +ubuntu-env --version=22.04

debian-env:
    # Generate a Debian environment of the given version
    ARG --required version
    FROM envs+debian --version=$version
    DO +SETUP

deb9.2-env:
    FROM +debian-env --version=9.2

deb8.1-env:
    FROM +debian-env --version=8.1

deb10-env:
    FROM +debian-env --version=10.0

c6-env:
    # CentOS 6 (Equivalent to RHEL 6)
    FROM envs+centos --version=6
    DO +SETUP

c7-env:
    # CentOS 7 (Equivalent to RHEL 7)
    FROM envs+centos --version=7
    DO +SETUP

rl8-env:
    # Rocky Linux 8 (Equivalent to RHEL 8)
    FROM envs+rockylinux --version=8
    DO +SETUP

arch-env:
    FROM envs+arch
    DO +SETUP

amzn2-env:
    FROM envs+amazonlinux --version=2
    DO +SETUP

suse-env:
    FROM envs+common-base --from=opensuse/leap --version=42
    DO +SETUP

TEST_WITH_CSE:
    # Run the client-side encryption tests with the appropriate daemon processes
    # NOTE: Requires a 'secrets.json' file for key management secrets
    COMMAND
    GIT CLONE https://github.com/mongodb-labs/drivers-evergreen-tools drivers-evergreen-tools
    # The arguments correspond to the environment variables that control the tests.
    ARG --required MONGODB_VERSION
    ARG --required TOPOLOGY
    ARG IPV4_ONLY
    ARG AUTH
    ARG AUTHSOURCE
    ARG SSL
    ARG ORCHESTRATION_FILE
    ARG OCSP
    ARG REQUIRE_API_VERSION
    # Run the tests:
    ENV CLIENT_SIDE_ENCRYPTION=ON
    COPY .evergreen/test-with-cse.sh .
    COPY secrets.json .
    RUN --no-cache bash -x test-with-cse.sh

COMPILE_SH:
    COMMAND
    COPY --dir .evergreen/ .
    ARG conf_flags
    ARG env_vars
    RUN --mount=type=cache,target=/root/.ccache \
        env CHECK_LOG=ON \
            DEBUG=ON \
            MAKEFLAGS=-j8 \
            "EXTRA_CONFIGURE_FLAGS=${conf_flags}" \
            ${env_vars} \
        /bin/sh -l .evergreen/compile.sh

BUILD:
    COMMAND

    COPY --dir \
        .git/ \
        build/ \
        generate_uninstall/ \
        .evergreen/ \
        src/ \
        orchestration_configs/ \
        CMakeLists.txt \
        COPYING NEWS README.rst CONTRIBUTING.md THIRD_PARTY_NOTICES \
        .

    # The version that we are building
    ARG build_version=1.20.0-dev

    # Only build contents that are committed to git
    ARG --required git_reset

    # Whether we build with client-side-encryption
    ARG --required client_side_encryption

    # Toggle debug ON/OFF
    ARG --required debug
    ENV DEBUG=$debug

    # Set C compilation flags
    ARG cflags
    ENV CFLAGS=$cflags

    # Enable parallelism for Make invocations
    ENV MAKEFLAGS=-j8

    IF $git_reset
        RUN git clean -fdx && git checkout .
    END

    ENV _base_configure_flags="-DBUILD_VERSION=$build_version -DENABLE_PIC=ON -DENABLE_CLIENT_SIDE_ENCRYPTION=$client_side_encryption -DENABLE_EXTRA_ALIGNMENT=OFF"

    # Build libmongocrypt if we are using client-side encryption
    IF test "$client_side_encryption" = "ON"
        DO +COMPILE_SH --env_vars=SKIP_MOCK_TESTS=ON --conf_flags "$_base_configure_flags -DENABLE_MONGOC=OFF"
        ENV COMPILE_LIBMONGOCRYPT=ON
        RUN rm CMakeCache.txt
    END

    # All possible build flags are specified here as required arguments.
    ARG --required check_log
    ENV CHECK_LOG=$check_log
    # Skip tests (note: SKIP_MOCK_TESTS just skips all tests)
    ARG --required skip_tests
    ENV SKIP_MOCK_TESTS=$skip_tests
    # SSL option
    ARG --required ssl
    ENV SSL=$ssl
    # ZSTD option
    ARG --required zstd
    ENV ZSTD=$zstd
    # C compiler
    ARG --required cc
    ENV CC=$cc
    # Target zlib
    ARG --required zlib
    ENV ZLIB=$zlib
    # Target arch
    ARG --required arch
    ENV MARCH=$arch
    # Snappy
    ARG --required snappy
    ENV SNAPPY=$snappy
    # SASL
    ARG --required sasl
    ENV SASL=$sasl
    # Toggle Valgrind
    ARG --required valgrind
    ENV VALGRIND=$valgrind

    DO +COMPILE_SH --conf_flags "$_base_configure_flags"

build:
    ARG --required env
    FROM "+${env}-env"
    # Reasonable default arguments for the build:
    ARG --required git_reset
    ARG client_side_encryption=ON
    ARG debug=ON
    ARG check_log=ON
    ARG sasl=SSPI
    ARG ssl=OPENSSL
    ARG zlib=BUNDLED
    ARG zstd=ON
    ARG snappy=ON
    ARG arch=native
    ARG skip_tests=OFF
    ARG cc=cc
    WORKDIR /s
    DO +BUILD \
        --client_side_encryption=$client_side_encryption \
        --git_reset=$git_reset \
        --debug=$debug \
        --check_log=$check_log \
        --valgrind=OFF \
        --skip_tests=$skip_tests \
        --sasl=$sasl \
        --ssl=$ssl \
        --zlib=$zlib \
        --zstd=$zstd \
        --snappy=$snappy \
        --arch=$arch \
        --cc=$cc

debug-compile-asan-clang-openssl:
    ARG --required env
    # Build with ASan and Clang, no tests:
    FROM +build \
        --env=$env \
        --cc=clang \
        --ssl=OPENSSL \
        --cflags="-fsanitize=address -fno-omit-frame-pointer" \
        --skip_tests=ON \
        --zstd=OFF

test-asan:
    # Obtain the build:
    ARG --required env
    FROM +build \
        --env=$env \
        --skip_tests=ON \
        --cflags="-fsanitize=address -fno-omit-frame-pointer"
    # Test arguments
    ARG mdb_version=5.0
    ARG topology=sharded_cluster
    ARG ssl=nossl
    ARG auth=noauth
    DO +TEST_WITH_CSE \
        --MONGODB_VERSION=$mdb_version \
        --TOPOLOGY=$topology \
        --SSL=$ssl \
        --AUTH=$auth

test:
    ARG --required env
    ARG --required git_reset
    FROM +build --env=$env --skip_tests=ON --git_reset=$git_reset
    DO +TEST_WITH_CSE \
        --AUTH=noauth \
        --SSL=openssl \
        --TOPOLOGY=server \
        --MONGODB_VERSION=5.0

build-all:
    ARG skip_tests=OFF
    ARG --required git_reset
    BUILD +build --env=u22      --git_reset=$git_reset --skip_tests=$skip_tests
    BUILD +build --env=u20      --git_reset=$git_reset --skip_tests=$skip_tests
    BUILD +build --env=u18      --git_reset=$git_reset --skip_tests=$skip_tests
    BUILD +build --env=u16      --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF
    BUILD +build --env=u14      --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF
    BUILD +build --env=rl8      --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF --snappy=OFF
    BUILD +build --env=c7       --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF --snappy=OFF
    BUILD +build --env=c6       --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF --snappy=OFF
    BUILD +build --env=amzn2    --git_reset=$git_reset --skip_tests=$skip_tests            --snappy=OFF
    BUILD +build --env=arch     --git_reset=$git_reset --skip_tests=$skip_tests            --snappy=OFF
    BUILD +build --env=suse     --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF --snappy=OFF
    BUILD +build --env=deb10    --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF
    BUILD +build --env=deb9.2   --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF
    BUILD +build --env=deb8.1   --git_reset=$git_reset --skip_tests=$skip_tests --zstd=OFF

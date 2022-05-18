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
    ARG --required from
    ARG --required version
    FROM envs+$from --version=$version
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
            RUN __install snappy libzstd-devel python3-virtualenv jq
        END
        IF test 1 = $(( $__env_major_version >= 7 ))
            RUN __install clang python3 python3-pip
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
    ELSE IF __is_alpine
        RUN __install \
            openssl-dev gcc g++ musl-dev make pkgconf python3 jq git cmake \
            zstd-dev snappy-dev net-snmp-dev
        # Have the build use the system's CMake on Alpine
        ENV CMAKE=cmake
    END
    DO +GIT_CONFIG
    DO +GET_CMAKE

u14-env:
    # Ubuntu 14.04
    DO +SETUP --from=ubuntu --version=14.04

u16-env:
    # Ubuntu 16.04
    DO +SETUP --from=ubuntu --version=16.04

u18-env:
    # Ubuntu 18.04
    DO +SETUP --from=ubuntu --version=18.04

u20-env:
    # Ubuntu 20.04
    DO +SETUP --from=ubuntu --version=20.04

u22-env:
    # Ubuntu 22.04
    DO +SETUP --from=ubuntu --version=22.04

deb9.2-env:
    DO +SETUP --from=debian --version=9.2

deb8.1-env:
    DO +SETUP --from=debian --version=8.1

deb10-env:
    DO +SETUP --from=debian --version=10.0

c6-env:
    # CentOS 6 (Equivalent to RHEL 6)
    DO +SETUP --from=centos --version=6

c7-env:
    # CentOS 7 (Equivalent to RHEL 7)
    DO +SETUP --from=centos --version=7

rl8-env:
    # Rocky Linux 8 (Equivalent to RHEL 8)
    DO +SETUP --from=rockylinux --version=8

arch-env:
    DO +SETUP --from=arch --version=latest

amzn2-env:
    DO +SETUP --from=amazonlinux --version=2

suse-env:
    DO +SETUP --from=suse --version=42

alpine3.15-env:
    DO +SETUP --from=alpine --version=3.15.4

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

COPY_C_SOURCE:
    COMMAND
    ARG --required to
    ARG --required git_reset
    ARG git_checkout
    COPY --dir \
        .git/ \
        build/ \
        generate_uninstall/ \
        .evergreen/ \
        src/ \
        orchestration_configs/ \
        CMakeLists.txt \
        COPYING NEWS README.rst CONTRIBUTING.md THIRD_PARTY_NOTICES \
        "${to}"
    IF $git_reset || ! test -z "$git_checkout"
        RUN cd "$to" && git clean -fdx && git checkout -- .
        IF ! test -z "$git_checkout"
            RUN cd "$to" && git checkout "$git_checkout"
        END
        RUN cd "$to" && git submodule update --init --recursive
    END

BUILD:
    COMMAND

    ARG --required jobs
    ARG --required git_reset
    ARG git_checkout
    DO +COPY_C_SOURCE --to=. --git_reset=$git_reset --git_checkout=$git_checkout

    # The version that we are building
    ARG build_version=1.20.0-dev

    # Whether we build with client-side-encryption
    ARG --required client_side_encryption

    # Toggle debug ON/OFF
    ARG --required debug
    ENV DEBUG=$debug

    # Set C compilation flags
    ARG cflags
    ENV CFLAGS=$cflags

    # Enable parallelism for Make invocations
    ENV MAKEFLAGS="-j$jobs"

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
    ARG git_checkout
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
    ARG jobs=8
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
        --cc=$cc \
        --git_checkout=$git_checkout \
        --jobs=$jobs

    SAVE ARTIFACT install-dir install-dir

debug-compile-asan-clang-openssl:
    ARG --required env
    # Build with ASan and Clang, no tests:
    FROM "+${env}-env"
    WORKDIR /s
    ARG --required git_reset
    ARG git_checkout
    ARG jobs=8
    DO +BUILD \
        --client_side_encryption=OFF \
        --debug=ON \
        --check_log=ON \
        --snappy=OFF \
        --zstd=OFF \
        --cc=clang \
        --zlib=BUNDLED \
        --valgrind=OFF \
        --skip_tests=ON \
        --sasl=OFF \
        --ssl=OPENSSL \
        --arch=native \
        --git_reset=$git_reset \
        --git_checkout=$git_checkout \
        --jobs=$jobs

evg-tools:
    GIT CLONE https://github.com/mongodb-labs/drivers-evergreen-tools drivers-evergreen-tools
    SAVE ARTIFACT drivers-evergreen-tools

TEST:
    # Run the client-side encryption tests with the appropriate daemon processes
    # NOTE: Requires a 'secrets.json' file for key management secrets
    COMMAND
    COPY --dir +evg-tools/drivers-evergreen-tools .
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
    ARG LOADBALANCED
    ARG ASAN
    ARG CLIENT_SIDE_ENCRYPTION=ON
    # Run the tests:
    COPY .evergreen/test-with-cse.sh .
    COPY --if-exists secrets.json .
    RUN --no-cache bash -x test-with-cse.sh

full-test:
    ARG --required env
    ARG --required git_reset
    ARG git_checkout
    ARG skip_unit_tests=OFF
    FROM +build --env=$env --skip_tests=$skip_unit_tests --git_reset=$git_reset --git_checkout=$git_checkout
    ARG mdb_version=5.0
    ARG topology=server
    ARG ssl=openssl
    ARG auth=noauth
    DO +TEST \
        --MONGODB_VERSION=$mdb_version \
        --TOPOLOGY=$topology \
        --SSL=$ssl \
        --AUTH=$auth \
        --ASAN=on

build-all:
    # Note: At time of writing, Earthly has no parallelization limits, so this
    #       will start a lot of simultaneous work!
    ARG --required git_reset
    ARG skip_tests=OFF
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

build-php:
    ARG --required env
    FROM "+${env}-env"
    IF __is_debian_based
        RUN __install "php7*-dev"
    ELSE IF __is_redhat_based
        RUN __install php-devel autoconf
    END

    ARG --required git_reset
    ARG php_git_ref=master
    WORKDIR /s
    GIT CLONE \
        --branch=$php_git_ref \
        https://github.com/mongodb/mongo-php-driver.git \
        /s/mongo-php-driver
    WORKDIR /s/mongo-php-driver
    RUN rm -rf src/libmongoc
    DO +COPY_C_SOURCE \
        --to=/s/mongo-php-driver/src/libmongoc \
        --git_reset=$git_reset \
        --git_checkout=""

    RUN phpize
    RUN bash ./configure --enable-mongodb-developer-flags
    ARG jobs=8
    RUN make -j "$jobs"

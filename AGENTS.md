# AGENTS.md

This file provides guidance to AI coding agents working with code in this repository.

## Build System

This project uses CMake.

Use `cmake-build/` as the CMake binary directory unless otherwise specified by the user. When the user specifies a custom binary directory, always use that directory - do not fall back to `cmake-build/`.

> [!IMPORTANT]
> Despite `build/` being a common choice for a CMake binary directory name, that is not recommended in this repository because the `build/` directory is tracked by Git.

The typical configure and build steps (for release and installation):

```bash
cmake -D CMAKE_BUILD_TYPE=RelWithDebInfo -B cmake-build
cmake --build cmake-build
```

The optional install step:

```bash
cmake --install cmake-build
```

> [!IMPORTANT]
> For multi-configuration generators (e.g. "Visual Studio *", "Ninja Multi-Config", etc.), use `--config <config>` during the build, install, and test steps instead of `CMAKE_BUILD_TYPE=<config>`.
> The `CMAKE_BUILD_TYPE` option will be ignored by the configuration step.
> Only use `CMAKE_BUILD_TYPE` with single-configuration generators (e.g. Makefile Generators, Ninja, etc.).

Key CMake configuration options (given `option=(default|alternatives...)`):

- `-G <generator-name>`: specify a build system generator.
- `-D CMAKE_PREFIX_PATH:PATH=<mongo-c-driver-prefix>`: specify installation prefixes to search with `find_*()` CMake commands (e.g., to link with `libmongocrypt`).
- `-D CMAKE_INSTALL_PREFIX:PATH=<install-prefix>`: install directory used by `install()`. Defaults to:
  - The `CMAKE_INSTALL_PREFIX` environment variable when set (with CMake 3.29 or newer).
  - `/usr/local` on UNIX platforms.
  - `C:/Program Files/${PROJECT_NAME}` on Windows.
  - Use `build/install/` as the custom install prefix when system modification is undesirable or disallowed by the user.
- `-D CMAKE_BUILD_TYPE:STRING=<config>`: specify the build type on single-configuration generators.
- `-D BUILD_SHARED_LIBS:BOOL=(ON|OFF)`: specify whether to build shared (`ON`) or static (`OFF`) libraries.

**Build performance:** Ninja parallelizes builds across all available cores by default; to cap the job count, set `CMAKE_BUILD_PARALLEL_LEVEL=<N>` in the environment before running `cmake --build`.

> [!NOTE]
> `.evergreen/scripts/compile.sh` is the authoritative reference for CI configure-build-install routines. Consult it for details on Ninja generator selection, ccache integration, sanitizer flags (`MONGO_SANITIZE`), client-side encryption (`ENABLE_CLIENT_SIDE_ENCRYPTION`), and other platform-specific options.

## Running Tests

The typical configure and build steps for testing:

```bash
cmake -D CMAKE_BUILD_TYPE=Debug -B cmake-build
cmake --build cmake-build --target test-libmongoc
```

> [!IMPORTANT]
> The "Debug" config type is recommended for local testing and development.
> If the project was already built with a different build type and the user has not requested a change, preserve the existing `CMAKE_BUILD_TYPE` rather than switching to `Debug`, and advise the user that switching to `Debug` is recommended for local testing and development.
> This configure step replaces the release/installation configure above - running both is unnecessary when developing.

Test executables are excluded from the `ALL` CMake target, so it is necessary to specify a target with the `--target` flag when building tests. The target `test-libmongoc` contains the majority of the test cases. The target `mongo_c_driver_tests` is a custom CMake target that can be used to build all test executables.

Most tests require a live MongoDB server. Server-dependent tests are skipped when no server is available.

Test executables include:

- `test-libmongoc`: Most of the tests for `libmongoc` and `libbson`. Uses a custom test framework; see the `running-test-libmongoc` skill in `.agents/skills/` for how to build, run, filter, and debug these tests.
- `test-mongoc-gssapi`: GSSAPI / Kerberos authentication tests. Requires a configured Kerberos environment (via `MONGOC_TEST_GSSAPI_HOST` / `MONGOC_TEST_GSSAPI_USER`) and connects concurrently from multiple threads.
- `test-sfp`: Connectivity tests against the Atlas Secure Front-End Processor (SFP), exercising unauthenticated, SCRAM, and X.509 auth across baseline, compressed, and Server API variants.
- `test-mongoc-cache`: End-to-end test for the OCSP response cache (Linux only). Confirms a revoked certificate stays revoked (TLS handshake keeps failing) from the cached OCSP response even after the OCSP responder is replaced with a valid one; it pauses itself with `SIGSTOP` so the harness can swap responders between connection attempts.
- `test-azurekms`: Client-Side Field Level Encryption (CSFLE) test for automatic Azure KMS credentials. Creates a data key with an empty `azure` KMS provider so credentials are obtained from the Azure VM-assigned managed identity. Must run on a configured Azure VM.
- `test-gcpkms`: CSFLE test for automatic GCP KMS credentials. Creates a data key with an empty `gcp` KMS provider so credentials are obtained from a GCP attached service account. Must run on a configured GCP VM.
- `test-awsauth`: `MONGODB-AWS` authentication mechanism tests, intended to run within an AWS ECS task or EC2 instance.

## Architecture

The repository provides several public, private, and vendored libraries, each under `src/`:

### Public Libraries

#### `libbson`

A public standalone BSON document library with no MongoDB dependency.

#### `libmongoc`

The public MongoDB C Driver. Depends on the `libbson` library.

#### `kms-message`

A library used to generate requests for Amazon Web Services Key Management Service (KMS) and Azure Key Vault.

### Private Libraries

#### `mlib` (found under `src/common/src/mlib`)

Facilities for checked arithmetic, macros, date/time, integer utilities, OS headers, strings, testing, and vector containers. Used by both `libbson` and `libmongoc`.

#### `common`

Provides various utilities such as concurrency primitives and JSON serialization. Technically not a library, but a collection of private headers and source files that are compiled with both `libbson` and `libmongoc`.

### Vendored Third-Party Libraries

There are several third-party libraries in the repository source tree to enable self-contained builds:

| Repository | Source Tree Location |
|---|---|
| https://github.com/mnunberg/jsonsl | `src/libbson/src/jsonsl` |
| https://github.com/JuliaStrings/utf8proc | `src/utf8proc-<version>` |
| https://github.com/troydhanson/uthash | `src/uthash/uthash-<version>` |
| https://github.com/madler/zlib | `src/zlib-<version>` |

### C Standard

Uses C99. Contributions shall not use features from newer standards.

## Before Committing

Always format before committing:

```bash
uv run --frozen tools/format.py           # C/C++ source files.
uv run --frozen tools/ruff-format-all.sh  # Python scripts.
uv run --frozen tools/shfmt-format-all.sh # Shell scripts.
```

`--frozen` ensures the pinned lockfile is used; if it fails with a lockfile error, update it with `uv sync`.

## Contributing

See `CONTRIBUTING.md` for guidance on:

- Indentation
- Error codes and domains
- API/ABI policy
- Documentation
- In-depth testing

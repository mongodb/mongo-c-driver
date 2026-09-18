# AGENTS.md

This file provides guidance to AI coding agents working with code in this repository.

## Build System

This project uses CMake.

`<build-dir>` below stands for the CMake binary directory. Use whichever
directory the user specifies. `cmake-build/` and `_build/` are both already
listed in `.gitignore`.

> [!IMPORTANT]
> Do not use `build/` as the binary directory. Unlike in most CMake projects, `build/` here is a tracked source directory holding the project's CMake modules, so configuring into it mixes build output into the working tree.

The typical configure and build steps (for release and installation):

```bash
cmake -D CMAKE_BUILD_TYPE=RelWithDebInfo -B <build-dir>
cmake --build <build-dir>
```

The optional install step:

```bash
cmake --install <build-dir>
```

Key CMake configuration options specific to this project (given `option=(default|alternatives...)`):

- `-D CMAKE_PREFIX_PATH:PATH=<libmongocrypt-prefix>`: specify installation prefixes to search with `find_*()` CMake commands (e.g., to link with `libmongocrypt`).
- `-D ENABLE_CLIENT_SIDE_ENCRYPTION:STRING=(AUTO|ON|OFF)`: In-Use Encryption support. Requires additional support libraries.
- `-D ENABLE_MAINTAINER_FLAGS:BOOL=(OFF|ON)`: stricter build-time checks, including the warning set that CI builds with. A default build does not enable these.
- `-D MONGO_SANITIZE:STRING=<list>`: semicolon/comma-separated list of sanitizers to build with (e.g. `address,undefined`).

> [!TIP]
> Setting the environment variable `MONGODB_DEVELOPER` to a true value changes the *defaults* of many of this project's settings to development-appropriate values: on non-MSVC compilers it turns `ENABLE_MAINTAINER_FLAGS` on and defaults `MONGO_SANITIZE` to `address,undefined`. This can explain build behavior that otherwise looks surprising. See `build/cmake/MongoSettings.cmake`.

> [!NOTE]
> `.evergreen/scripts/compile.sh` is the authoritative reference for CI configure-build-install routines. Consult it for details on Ninja generator selection, ccache integration, sanitizer flags (`MONGO_SANITIZE`), client-side encryption (`ENABLE_CLIENT_SIDE_ENCRYPTION`), and other platform-specific options.

## Running Tests

The typical configure and build steps for testing:

```bash
cmake -D CMAKE_BUILD_TYPE=Debug -B <build-dir>
cmake --build <build-dir> --target test-libmongoc
```

> [!IMPORTANT]
> The "Debug" config type is recommended for local testing and development, and this configure step replaces the release/installation configure above - running both is unnecessary when developing.
> If the project was already built with a different build type and the user has not requested a change, preserve the existing `CMAKE_BUILD_TYPE` rather than switching to `Debug`, and advise the user that switching to `Debug` is recommended.

Test executables are excluded from the `ALL` CMake target, so it is necessary to specify a target with the `--target` flag when building tests. The target `test-libmongoc` contains the majority of the test cases. The target `mongo_c_driver_tests` is a custom CMake target that can be used to build all test executables.

Every `test-libmongoc` case is additionally registered as an individual CTest test, so the suite can be run either through `ctest` or by invoking the executable directly. The `running-test-libmongoc` skill in `.agents/skills/` covers both paths, along with test naming, filtering, tags, and debugging.

Most tests require a live MongoDB server. Server-dependent tests may be skipped with the environment variable `MONGOC_TEST_SKIP_LIVE`. The other `MONGOC_TEST_*` variables that configure the suite are documented under *Testing* in `CONTRIBUTING.md`.

Test executables include:

- `test-libmongoc`: Most of the tests for `libmongoc` and `libbson`. Uses a custom test framework, with its own naming and filtering rules (see the skill referenced above).
- `test-mongoc-gssapi`: GSSAPI / Kerberos authentication tests. Requires a configured Kerberos environment (via `MONGOC_TEST_GSSAPI_HOST` / `MONGOC_TEST_GSSAPI_USER`) and connects concurrently from multiple threads.
- `test-sfp`: Connectivity tests against the Atlas Secure Front-End Processor (SFP), exercising unauthenticated, SCRAM, and X.509 auth across baseline, compressed, and Server API variants.
- `test-mongoc-cache`: End-to-end test for the OCSP response cache (Linux only). Confirms a revoked certificate stays revoked (TLS handshake keeps failing) from the cached OCSP response even after the OCSP responder is replaced with a valid one; it pauses itself with `SIGSTOP` so the harness can swap responders between connection attempts.
- `test-azurekms`: Client-Side Field Level Encryption (CSFLE) test for automatic Azure KMS credentials. Creates a data key with an empty `azure` KMS provider so credentials are obtained from the Azure VM-assigned managed identity. Must run on a configured Azure VM.
- `test-gcpkms`: CSFLE test for automatic GCP KMS credentials. Creates a data key with an empty `gcp` KMS provider so credentials are obtained from a GCP attached service account. Must run on a configured GCP VM.
- `test-awsauth`: `MONGODB-AWS` authentication mechanism tests, intended to run within an AWS ECS task or EC2 instance.

### Test Infrastructure

- `src/libmongoc/tests/test-libmongoc.c` — the test runner and its internal `TestSuite` framework.
- `src/libmongoc/tests/mock_server/` — mock MongoDB server for protocol-level tests.
- `src/libbson/tests/` — libbson unit tests.
- Spec tests are JSON files under `src/libmongoc/tests/json/` and `src/libbson/tests/json/`.

## Architecture

The repository provides several public, private, and vendored libraries, each under `src/`. Both public libraries build from the same CMake project.

### Public Libraries

- **`libbson`** (`src/libbson/src/bson/`): a public standalone BSON document library with no MongoDB dependency.
- **`libmongoc`** (`src/libmongoc/src/mongoc/`): the public MongoDB C Driver. Depends on the `libbson` library.

### Private Libraries

- **`kms-message`** (`src/kms-message/`): creates signed Amazon Web Services (AWS) requests for the `MONGODB-AWS` auth mechanism, and handles the KMS wire protocol used by In-Use Encryption.
- **`mlib`** (`src/common/src/mlib/`): header-only facilities for checked arithmetic, macros, date/time, integer utilities, OS headers, strings, testing, and vector containers. Used by both `libbson` and `libmongoc`. Key headers: `str.h` (string type), `ckdint.h` (checked integer arithmetic), `vec.th`/`str_vec.h` (generic vectors), `cmp.h` (comparisons), `loop.h` (iteration helpers).
- **`common`** (`src/common/src/`): various utilities such as concurrency primitives and JSON serialization. Technically not a library, but a collection of private headers and source files that are compiled with both `libbson` and `libmongoc`.

`mlib/ckdint.h` is a C99-compatible backport of the C23 `stdckdint.h` API (`mlib_add`, `mlib_sub`, `mlib_mul`, `mlib_narrow`, and the `mlib_assert_*` variants). Prefer these over raw arithmetic and narrowing casts wherever overflow or truncation is possible. See `src/common/src/mlib/ckdint.md` for usage details.

### BSON DSL (`common-bson-dsl-private.h`)

An internal macro DSL for constructing and parsing `bson_t` objects declaratively. Use `bsonBuild`/`bsonBuildDecl` for writing and `bsonParse`/`bsonVisitEach` for reading. See `src/common/src/bson-dsl.md` for full documentation.

### Header Conventions

Public headers have no `private` suffix (e.g., `mongoc-client.h`). Internal headers are named `*-private.h`. The `_private.h` suffix indicates the struct definition is exposed for internal use only.

### Navigating libmongoc

Implementation files are named `mongoc-<subsystem>*.c`, so the file listing is
itself a serviceable index: `mongoc-uri.c` (connection string parsing),
`mongoc-collection.c` / `mongoc-database.c`, `mongoc-bulkwrite.c` /
`mongoc-bulk-operation.c`, `mongoc-client-session.c` (sessions and
transactions), `mongoc-gridfs*.c`, `mongoc-structured-log.c`, and so on. The
`mcd-` prefix marks assorted newer internal components rather than one subsystem
(`mcd-rpc.c` is the wire protocol message representation).

The relationships that the file names do *not* convey:

- **Client, pool, and topology.** A single-threaded `mongoc_client_t` owns its
  `mongoc_topology_t` directly, while every client handed out by a
  `mongoc_client_pool_t` shares the pool's single topology. Which objects are
  shared between threads follows from this split.
- **SDAM** spans more than `mongoc-topology*.c`: `mongoc-server-description.c`,
  `mongoc-server-monitor.c`, and `mongoc-topology-background-monitoring.c` are
  all part of it.
- **Cluster** (`mongoc-cluster.c`) owns the open connections to servers and
  dispatches commands — the layer between a client operation and the wire.
  Authentication mechanisms are *also* here, as `mongoc-cluster-<mechanism>.c`
  (`-aws`, `-cyrus`, `-oidc`, `-sasl`, `-sspi`), alongside `mongoc-scram.c`.
- **Cursor** is one public type over several backends: `mongoc-cursor-find.c`,
  `-cmd.c`, `-array.c`, `-change-stream.c`.
- **Streams** are layered behind the `mongoc_stream_t` vtable: a TLS stream
  (`mongoc-stream-tls-*.c`, one per TLS backend) wraps a base socket or buffered
  stream.
- **In-Use Encryption** is split between `mongoc-client-side-encryption.c` (the
  public API) and `mongoc-crypt.c` (the binding to libmongocrypt).
- **APM** (`mongoc-apm.c`) exposes callbacks, covering command, server/topology,
  and heartbeat events. There are no connection-pool (CMAP) callbacks.

### Vendored Third-Party Libraries

There are several third-party libraries in the repository source tree to enable
self-contained builds:

| Repository | Source Tree Location (where `<version>` is a placeholder for the version number) |
|---|---|
| https://github.com/mnunberg/jsonsl | `src/libbson/src/jsonsl` |
| https://github.com/JuliaStrings/utf8proc | `src/utf8proc-<version>` |
| https://github.com/troydhanson/uthash | `src/uthash/uthash-<version>` |
| https://github.com/madler/zlib | `src/zlib-<version>` |

### C Standard

Uses C99. Contributions shall not use features from newer standards.

## Code Style

See *Coding Style* in `CONTRIBUTING.md` and `.clang-format` for indentation. In addition:

- Format using formatting scripts. Don't try to manipulate things by hand when
  you can just run the formatters described below.
- **clangd**: requires configuring with `-D CMAKE_EXPORT_COMPILE_COMMANDS=ON`,
  which is not enabled by default. If clangd reports unknown headers or missing
  symbols, check that `<build-dir>/compile_commands.json` exists before looking
  anywhere else. You may also require a `.clangd` configuration file that
  directs clangd where to find the compilation database.
- Include directives for `mlib/`, `bson/`, and `mongoc/` headers use angle
  brackets (`<mlib/str.h>`, not `"mlib/str.h"`). This will be enforced
  automatically by the formatting scripts. Be aware that formatting may
  rearrange `#include` directives.
- New `.c`/`.h` files carry an Apache 2.0 license header. Copy the boilerplate
  from an existing file rather than composing it from memory.

## Documentation

Documentation is authored as separate `.rst` files under `src/libbson/doc` and `src/libmongoc/doc`, and generated with Sphinx. There is a document for each public API type (e.g., `src/libmongoc/doc/mongoc_client_t.rst`) and function (e.g., `src/libmongoc/doc/mongoc_client_new.rst`), so adding a new public symbol requires adding its `.rst` file.

See *Documentation* in `CONTRIBUTING.md` for the `sphinx-build` and `sphinx-autobuild` invocations, and for the comment-block convention used on complex internal functions.

## Before Committing

Always format before committing:

```bash
uv run --frozen tools/format.py           # C/C++ source files.
uv run --frozen tools/ruff-format-all.sh  # Python scripts.
uv run --frozen tools/shfmt-format-all.sh # Shell scripts.
```

## Contributing

See `CONTRIBUTING.md` for guidance on:

- AI usage (all changes require human review; low-effort AI-generated PRs may be rejected)
- Portability across supported platforms
- Indentation
- Error codes and domains
- API/ABI policy
- Documentation
- In-depth testing

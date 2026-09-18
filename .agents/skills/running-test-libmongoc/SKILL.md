---
name: running-test-libmongoc
description: Use when running, filtering, or debugging tests in the test-libmongoc executable of the MongoDB C Driver — the custom TestSuite framework in src/libmongoc/tests/TestSuite.h with its own flags (-l, -d, -f), test-naming rules, tags, and JSON spec tests, whether run directly or through CTest.
---

# Running `test-libmongoc`

## Overview

`test-libmongoc` is the executable holding most `libmongoc` and `libbson` tests. It uses a **custom** test framework (`src/libmongoc/tests/TestSuite.h`), not a mainstream one, so its invocation, filtering, and test-naming rules are unique. This skill explains how to run and select tests with it.

Build it first — it is not part of the default `ALL` target (`<build-dir>` is the CMake binary directory):

```bash
cmake --build <build-dir> --target test-libmongoc
```

There are two ways to run the resulting tests, and they suit different jobs:

| | Use for |
|---|---|
| **CTest** (`ctest`) | Running many cases, parallelism (`-j`), selecting by label, per-case timeouts, and machine-readable results. |
| **The executable directly** | Running or debugging a single case, where `-d` / `-f` and immediate output matter. |

## Running via CTest

Every test case is registered as an individual CTest test named `mongoc/<case-name>`:

```bash
# List matching tests without running them:
ctest --test-dir <build-dir> -N -R "/server_selection/"

# Run the matching tests:
ctest --test-dir <build-dir> --output-on-failure -R "/server_selection/"
```

`-R` is a regex over the full CTest test name, which includes the `mongoc` prefix and the case's leading `/`.

Each case's `[...]` tags (see [Finding a test's name](#finding-a-tests-name)) are translated into CTest properties by `build/cmake/LoadTests.cmake`:

| Tag | Becomes |
|-----|---------|
| any tag | a CTest **label**, so `ctest -L "lock:live-server"` selects by tag. All generated cases also get the label `test-libmongoc-generated`. |
| `lock:<name>` | a `RESOURCE_LOCK`, serializing cases that share a lock name against each other. |
| `uses:<name>` | a required test fixture, `mongoc/fixtures/<name>`. |
| `timeout:<seconds>` | that `TIMEOUT`. **Cases without this tag get a 10 second timeout**, so a genuinely slow test must declare one or it will be killed. |

Cases run with the working directory set to the source root, and a case that reports itself skipped is recorded as skipped rather than passed. The `MONGOC_TEST_*` environment variables documented in `CONTRIBUTING.md` apply to `ctest` runs as well.

## Running the executable directly

Typical invocation:

```bash
<build-dir>/src/libmongoc/test-libmongoc -d -f -l "<test-name-or-pattern>"
```

| Flag | Meaning |
|------|---------|
| `-l` | Run tests matching a name or pattern. **Repeatable** — pass `-l` multiple times to select several tests in one run. |
| `-d` | Print debug output. Useful when a test hangs. |
| `-f` | Do not fork a process per test; abort on the first error. |

Run `<build-dir>/src/libmongoc/test-libmongoc --help` for the full flag list.

### `-l` matching rules

Matching against a test's name (`TestSuite_TestMatchesName` in `TestSuite.c`) is **exact string**, with one exception: a **trailing** `*` makes it a prefix match. There is no other wildcard support.

- `-l "*aggregate"` does **not** work — a `*` is only special as the last character.
- A trailing `*` is a *prefix* match and over-matches siblings: `-l "/crud/unified/aggregate*"` also matches `/crud/unified/aggregate-let`, `/crud/unified/aggregate-merge`, etc. To run exactly one test, pass its full name without a `*` (e.g. `-l "/crud/unified/aggregate"`).

## Finding a test's name

A test's registration string may bundle a name and one or more space-separated `[...]` tags. At registration (`_V_TestSuite_AddFull`) the string is **split on the first space**: everything before it is the test's name, and the bracketed tags are stored separately as metadata. **`-l` matches the name only — never include a tag in the pattern.** Names come from one of two places:

1. **The second argument to a `TestSuite_Add*` call.** In `/loadbalanced/connect/single [lock:live-server]`,
   the name is `/loadbalanced/connect/single` and `[lock:live-server]` is a tag indicating the
   test needs a live MongoDB server. Match it with `-l "/loadbalanced/connect/single"`.
2. **A JSON spec test.** Its name is the path starting at (and including) the `/` after
   `src/libmongoc/tests/json`, with `.json` removed. Spec tests installed via
   `install_json_test_suite` also get a ` [lock:live-server]` tag appended and a
   `TestSuite_CheckLive` check, so they require a live server. Example:
   `src/libmongoc/tests/json/crud/unified/aggregate.json` has the name
   `/crud/unified/aggregate` (leading `/`, no tag) — match it with `-l "/crud/unified/aggregate"`.

Tip: `test-libmongoc --list-tests` prints every registered test name (all tests, unfiltered; tags are not shown). Pipe it through `grep` to find the exact name to pass to `-l`.

## Registration functions (server requirements)

Tests register with the suite through these functions; which one is used tells you whether a live server is required:

| Function | Requires live server? | Notes |
|----------|----------------------|-------|
| `TestSuite_Add` | No | Basic test. |
| `TestSuite_AddLive` | Yes | Needs a live MongoDB server. |
| `TestSuite_AddFull` | Depends | May use a context object or skip conditions. |

## Common mistakes

- **Including a `[...]` tag in the `-l` pattern** — tags are metadata, not part of the name; `-l` matches the name only. Use `-l "/crud/unified/aggregate"`, not `-l "/crud/unified/aggregate [lock:live-server]"`.
- **Dropping the leading `/` from a spec-test name** — the name is `/crud/unified/aggregate`, not `crud/unified/aggregate`. The leading slash is part of the name.
- **Assuming a trailing `*` matches exactly one test** — it is a prefix match and catches siblings. See the matching rules above.
- **Using a leading or interior wildcard** — `-l "*aggregate"` will not work; only a trailing `*` is supported.
- **Forgetting to build the target first** — `test-libmongoc` is not built by the default `ALL` target.
- **Expecting a fork-per-test by default when debugging** — add `-f` so the run aborts on the first failure instead of continuing, and `-d` to see where a hang occurs.
- **Reading a CTest timeout as a test failure** — an untagged case is killed at 10 seconds, so a slow-but-correct test looks like a failure until it declares a `timeout:` tag. Confirm by running that one case directly.
- **Passing an `-l` pattern to `ctest -R`, or vice versa** — `-l` is an exact/prefix match on the bare test name, while `-R` is a regex over the CTest name (which carries the `mongoc` prefix).
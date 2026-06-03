##################
API and ABI Policy
##################

This page describes the stability guarantees that the C driver project makes
about its public API and ABI across releases, how deprecation works, and what
is explicitly *not* covered by these guarantees.

.. note::

   This policy applies to both **libmongoc** and **libbson**.


.. _api-abi-policy.versioning:

Versioning Scheme
#################

The C libraries use a ``<major>.<minor>.<patch>`` versioning scheme that is
based on `Semantic Versioning <https://semver.org>`_. Prerelease and development
versions include a suffix to indicate that the contents of the libraries are
unstable and subject to change.

- A **major** version indicates overall breaking changes. Different major
  versions offer no compatibility guarantees in their API and ABI.
- A **minor** version indicates the addition of new APIs and features to the
  libraries, or possible deprecation of APIs. Programs written against earlier
  minor versions should continue to compile and link with a newer minor version
  with minimal or no changes.
- A **patch** version (also known as the "micro" version at some locations in
  the source code) indicates bugfixes and tweaks and will not deprecate or add
  any major features to libraries. Programs written against earlier patch
  versions should compile and link against newer patch versions with no changes
  unless security considerations require API or ABI changes.


.. _api-abi-policy.public-api:

What Is the Public API?
#######################

The public API consists of:

* Every symbol (function, type, macro, constant) declared in a **public header**
  — a header that is installed alongside the library and has no ``-private``
  suffix in its name (e.g. ``mongoc/mongoc-client.h``, ``bson/bson.h``), except
  for those noted below.
* Every symbol annotated with ``MONGOC_EXPORT`` or ``BSON_EXPORT``.
* The *documented* behavior of any API function.

The following are **not** part of the public API and carry **no stability
guarantee**:

* Headers with a ``-private.h`` suffix (e.g. ``mongoc-client-private.h``) and
  the contents thereof.
* Anything under ``src/common/`` (the internal shared utility library).
* Symbols prefixed with an underscore or otherwise documented as internal.
* The layout of any struct whose definition lives in a ``-private.h`` header,
  even when the opaque ``typedef`` is public.
* The content of ``src/kms-message/``, a vendored library with its own
  versioning.
* The *undocumented, incidental* behavior of any function.


Header Inclusion
****************

All library header ``#include`` directives should use the ``bson/`` or
``mongoc/`` directory prefix. Including unqualified header filenames by
modifying the header search paths is unsupported.

Header files that have the ``bson/bson-*`` or ``mongoc/mongoc-*`` (with the
hyphen) prefix should never be included directly, despite being part of the
public API. Only their contents are part of the public API, not the filename or
the physical location of the entities contained within. The content of these
headers may be moved in any version, or those headers may be removed entirely.
Instead, users should include the umbrella headers
``<bson/bson.h>``/``<mongoc/mongoc.h>`` or they may directly include one of the
headers without the ``bson-`` or ``mongoc-`` prefix in its filename.

By default, libbson and libmongoc headers are not installed as direct children
of the platform's default header include-search-path directory, and live in a
subdirectory that is qualified by the library version. Users should never use
this intermediate directory in their include directives, and should rely on
CMake or pkg-config to set the appropriate flags to update their
include-search-path to find the relevant headers in this versioned subdirectory.


.. _api-abi-policy.api:

API Stability
#############

Within a **major** version series, the project makes the following guarantees:

* **No removal** of public symbols — functions, types, macros, and constants
  present in a release remain available in all subsequent releases within the
  same major version.
* **No rename** of a public symbol without providing a compatibility alias for
  the original name.
* **No incompatible signature changes** — the parameter and return types of
  public functions do not change in ways that require callers to be updated.
  Note that type-qualifiers may be added or removed from parameter or return
  types in ways that do not affect existing callers.
* **No change in documented contract** — the observable behavior, preconditions,
  postconditions, and error semantics of a public function do not change in a way
  that causes a previously-correct caller to misbehave.
* **No change in the values** of public constants or enum members in a way that
  alters the semantics of existing programs.

As a result, a program that compiles cleanly against version ``X.Y.Z`` is
expected to compile and link without modification against any later ``X.Y'.Z'``
release in the same major series, provided the program only uses the public API.

Within a **minor** release, new public symbols may be added (see
:ref:`api-abi-policy.additions`). Within a **patch** release, no new public
symbols are introduced and existing behavior changes only to correct bugs.

Deprecation (see :ref:`api-abi-policy.deprecation`) is the only mechanism by
which a public symbol can eventually be removed; removal only occurs at a
**major** release boundary. Until a symbol is actually removed it remains fully
functional — not merely a stub.

.. rubric:: Bug fixes and behavior

A change that corrects a function's behavior to match its documented contract is
not an API break, even if some callers relied on the erroneous behavior.
Undocumented or incorrect behavior carries no stability guarantee.

.. rubric:: Macros

The observable behavior of a public macro is stable within a major version
series. The specific token sequence of a macro's expansion is not guaranteed and
may change between releases — for example, to replace a function-like macro with
an inline function or to add internal casts. Code that depends on a specific
expansion rather than on the macro's documented behavior is unsupported.

.. note::

  Whether an API function is implemented as a preprocessor macro is an
  implementation detail unless documented otherwise. Relying on whether an API
  function name is a macro is not supported.

  Do not attempt to take the address of an API function or perform an ``#ifdef``
  on an API function name, unless such an operation is said to be supported in
  the documentation.

.. rubric:: Error codes

New error codes and domain values may be introduced in any **minor** release.
Callers should handle unknown error codes gracefully rather than treating the set
of possible values as exhaustive.


.. _api-abi-policy.abi:

ABI Stability
#############

Except in the case of security patches, within a **major** version series, the
project guarantees the following:

* **No removal** of exported symbols, and no change to an exported symbol's
  link name.
* **No signature changes** to exported functions (parameter types, return type,
  calling convention), except for the possible addition of type-qualifiers to
  pointer parameters or removal of type-qualifiers from pointer return types.
* **No breaking changes** to the size or layout of any completely-defined struct
  reachable in the public API, even if the contents of that struct are not
  themselves part of the API. Where feasible, structs are kept opaque (accessed
  only through accessor functions) to preserve this freedom.

As a result, a program compiled against version ``X.Y.Z`` is expected to load
and run correctly against any later ``X.Y'.Z'`` shared library, provided the
program only uses the public API.

Additionally, the ``soname`` of a built dynamic library is part of the ABI and
guaranteed stable within a major version. The ``soname`` of the resulting
libraries includes the major version of the library, preventing collisions
between major versions. The CMake ``SOVERSION`` property of resulting dynamic
library artifacts is always equivalent to the major API version of the library.
(The ``soname`` and ``SOVERSION`` are not applicable to Windows DLLs.)

In the uncommon case that a security patch requires breaking any of the above
guarantees, it will be noted in the release notes for the corresponding version.

.. rubric:: Exported symbols

The ``BSON_EXPORT`` and ``MONGOC_EXPORT`` macros are used to add appropriate
attributes to external-linkage symbols that are visible in a dynamic library
build. The presence of such symbols in a built artifact is part of the ABI of
that artifact.

.. rubric:: Incomplete types

Many types (e.g. ``mongoc_client_t``) are intentionally incomplete in the public
API and are only passed/returned by address. Callers should never rely on the
size, alignment, or fields of such a type. Adding, removing, or rearranging the
content of such types is not an ABI break.

.. rubric:: Public aggregate types

A small number of structs expose their fields in public headers as part of their
public API (e.g. ``bson_iter_t``). Changes to these structs **are** ABI breaks
and are therefore prohibited within a **major** version series.

.. rubric:: Compilation settings

Certain preprocessor macros and CMake configure-time build parameters may affect
the ABI of the resulting build. When possible, programs should not attempt to
combine translation units together that are not compiled with the same set of
preprocessor definitions.

Users should not manually define preprocessor macros during their compilation
that could affect the content of the exposed library APIs. Instead, users should
rely on the CMake imported targets or pkg-config definitions to set the
appropriate compile/link settings for their program.

.. note::

   The project runs an ABI compliance check in CI (the ``abi-compliance-check``
   Evergreen task) that compares the current branch against the most recent
   stable release. Any unexpected breakage should be caught before merge.


.. _api-abi-policy.deprecation:

Deprecation Policy
##################

Symbols that need to be removed follow a two-step cycle:

1. **Mark deprecated.** Annotate the declaration with ``BSON_DEPRECATED("…")``
   or ``BSON_DEPRECATED_FOR(replacement)`` and update the documentation to note
   the deprecation and its replacement. This is a non-breaking change that may
   land in any **minor** release. **mongoc** also uses the ``BSON_DEPRECATED``
   macros (there is no ``MONGOC_DEPRECATED`` equivalent).

2. **Remove.** The symbol is removed no earlier than the *next major release*
   after it was deprecated, giving users at least one full major-version cycle
   to migrate.

A deprecated symbol must remain present and functional (not just a stub) until
it is removed.


.. _api-abi-policy.additions:

Adding New Symbols
##################

New public symbols may be added in any **minor** release. When adding a symbol:

1. Declare it in the appropriate public header.
2. For functions, annotate it with ``MONGOC_EXPORT`` or ``BSON_EXPORT``.
3. Add a ``.rst`` documentation page in ``src/libmongoc/doc/`` or
   ``src/libbson/doc/``.
4. Verify it appears in the ABI compliance report under *Added Symbols* (see
   the ``abi-compliance-check`` Evergreen task artifact).

Adding new symbols is **not** an ABI break. However, a new symbol that clashes
with a name a downstream project already defines privately can cause linker
confusion; prefer descriptive, namespaced names (``mongoc_`` or ``bson_``
prefix).


Other Notes
###########

Build configuration compatibility
*********************************

The build parameters are themselves subject to the same stability guarantees as
the C API, and follow the same deprecation policy.

The C API and ABI guarantees only hold within the same build configuration.
Attempting to mix build configurations is unsupported, as build settings may
affect the API or ABI of the resulting artifacts.


Pre-release versions
********************

Any release with a pre-release label (e.g. ``2.0.0-alpha1``) carries **no
stability guarantee**. Any changes that are part of a prerelease may be removed
or modified without notice between pre-release versions.


Experimental APIs
*****************

APIs that are documented as **experimental** may change or be removed in any
release, including patch releases, regardless of the usual policy. Such APIs
will be clearly marked in the documentation.


Build system integration API
****************************

The CMake config-file packages are the recommended way to import the project.
The CMake targets (``mongoc::shared``, ``bson::static``, etc.) and the
properties they expose are considered public and follow the same versioning
rules as the C API. However, the internal CMake helper modules under
``build/cmake/`` are not public and may change without notice.

The pkg-config files are also supported for building against the libraries, and
will ensure that the compilation settings match the ABI of the corresponding
binaries.

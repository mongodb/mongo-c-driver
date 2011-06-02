# MongoDB C Driver

This is a very basic MongoDB C driver. The goal is to be super strict for ultimate portability,
no dependencies, and generic embeddability.

Until the 1.0 release, this driver should be considered alpha. Keep in mind that the API will be in flux until then.

Please post tickets and improvements to [JIRA](http://jira.mongodb.org/browse/CDRIVER).

You'll need [JSON-C](http://oss.metaparadigm.com/json-c/) to compile all the unit tests, but it's not required for the main libraries.

# Building

First checkout the version you want to build. *Always build from a particular tag, since HEAD may be
a work in progress.* For example, to build version 0.3, run:

    git checkout v0.3

Then follow the build steps below.

## Building with scons:
    scons # this will produce libbson.a and libmongoc.a
    scons --c99 # this will use c99 mode in gcc (recommended)

## Building with gcc:
    gcc --std=c99 -Isrc src/*.c YOUR_APP.c # No -Ddefines are needed in c99 mode on little endien

## Running the tests
Make sure that you're running mongod on 127.0.0.1 on the default port (27017). The replica set
test assumes a replica set with at least three nodes running at 127.0.0.1 and starting at port
30000. Note that the driver does not recognize 'localhost' as a valid host name.

To compile and run the tests:

    scons test

You may optionally specify a remote server:

    scons test --test-server=123.4.5.67

You may also specify an alternate starting port for the replica set members:

    scons test --test-server=123.4.5.67 --seed-start-port=40000

# Custom defines
(Note: you must use the same flags to compile all apps and libs):

`MONGO_BIG_ENDIAN`             This must be defined if on a big endian architecture

one of these (defaults to unsigned char if neither is defined):

`MONGO_HAVE_BOOL`              Define this if your compiler has a plain 'bool' type

`MONGO_HAVE_STDBOOL`           Define this if you must include <stdbool.h> to get 'bool'

one of these (required if not using c99):

MONGO_HAVE_STDINT            Define this if you have <stdint.h> for int64_t
MONGO_HAVE_UNISTD            Define this if you have <unistd.h> for int64_t
MONGO_USE__INT64             Define this if '__int64' is your compiler's 64bit type (MSVC)
MONGO_USE_LONG_LONG_INT      Define this if 'long long int' is your compiler's 64bit type

# Error Handling
Most functions return MONGO_OK or BSON_OK on success and MONGO_ERROR or BSON_ERROR on failure.
Specific error codes and string are then stored in the `err` and `errstr` fields of the
`mongo_connection`, `bson_buffer`, and `bson` structs.

The error handling conventions are still a work in progress but will be
consistent and complete for the 0.5 release.

# TODO
* building on windows
* more documentation
* checking for $err in query results
* query helper for sort and hint
* explain and profiler helpers
* safe-mode modifications (maybe)
* cached ensure_index (maybe)

# CREDITS

* Gergely Nagy - Non-null-terminated string support.
* Josh Rotenberg - Initial Doxygen setup and a significant chunk of documentation.

# LICENSE

Unless otherwise specified in a source file, sources in this
repository are published under the terms of the Apache License version
2.0, a copy of which is in this repository as APACHE-2.0.txt.

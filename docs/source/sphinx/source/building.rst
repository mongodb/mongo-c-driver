Building the MongoDB C Driver
=============================

First checkout the version you want to build. *Always build from a particular tag, since HEAD may be
a work in progress.* For example, to build version 0.4, run:

.. code-block:: bash

    git checkout v0.4

Then follow the build steps below.

Compile options with custom defines
----------------------------------

Before compiling, you should note the following compile options.

For big-endian support, define:

- ``MONGO_BIG_ENDIAN``

If your compiler has a plain ``bool`` type, define:

- ``MONGO_HAVE_BOOL``

Alternatively, if you must include ``stdbool.h`` to get ``bool``, define:

- ``MONGO_HAVE_STDBOOL``

If you're not using C99, then you must choose your 64-bit integer type by
defining one of these:

- ``MONGO_HAVE_STDINT`` - Define this if you have ``<stdint.h>`` for int64_t.
- ``MONGO_HAVE_UNISTD`` - Define this if you have ``<unistd.h>`` for int64_t.
- ``MONGO_USE__INT64``  - Define this if ``__int64`` is your compiler's 64bit type (MSVC).
- ``MONGO_USE_LONG_LONG_INT`` - Define this if ``long long int`` is your compiler's 64-bit type.

Building with Make:
-------------------

If you're building the driver on UNIX-like platforms, including on OS X,
then you can build with ``make``.

To compile the driver, run:

.. code-block:: bash

    make

This will build the following libraries:

* libbson.a
* libbson.so (libbson.dylib)
* libmongoc.a
* lobmongoc.so (libmongoc.dylib)

You can install the librares with make as well:

.. code-block:: bash

    make install

And you can run the tests:

.. code-block:: bash

    make test

By default, ``make`` will build the project in ``c99`` mode. If you want to change the
language standard, set the value of STD. For example, if you want to build using
the ANSI C standard, set STD to c89:

.. code-block:: bash

    make STD=c89

Once you've built and installed the libraries, you can compile the sample
with ``gcc`` like so:

.. code-block:: bash

    gcc --std=c99 -I/usr/local/include -L/usr/local/lib -o example docs/example/example.c -lmongoc

If you want to statically link the program, add the ``-static`` option:

.. code-block:: bash

    gcc --std=c99 -static -I/usr/local/include -L/usr/local/lib -o example docs/example/example.c -lmongoc

Then run the program:

.. code-block:: bash

    ./example

Building with SCons:
--------------------

You may also build the driver using the Python build utility, SCons_.
This is required if you're building on Windows. Make sure you've
installed SCons, and then from the project root, enter:

.. _SCons: http://www.scons.org/

.. code-block:: bash

    scons

This will build static and dynamic libraries for both ``BSON`` and for the
the driver as a complete package. It's recommended that you build in C99 mode
with optimizations enabled:

.. code-block:: bash

    scons --c99

Once you're built the libraries, you can compile a program with ``gcc`` like so:

.. code-block:: bash

    gcc --std=c99 -static -Isrc -o example docs/example/example.c libmongoc.a

Platform-specific features
--------------------------

TODO.


Dependencies
------------

The driver itself has no dependencies, but one of the tests shows how to create a JSON-to-BSON
converter. For that test to run, you'll need JSON-C_.

.. _JSON-C: http://oss.metaparadigm.com/json-c/

Test suite
----------

Make sure that you're running mongod on 127.0.0.1 on the default port (27017). The replica set
test assumes a replica set with at least three nodes running at 127.0.0.1 and starting at port
30000. Note that the driver does not recognize 'localhost' as a valid host name.

With make:

.. code-block:: bash

    make test

To compile and run the tests with SCons:

.. code-block:: bash

    scons test

You may optionally specify a remote server:

.. code-block:: bash

    scons test --test-server=123.4.5.67

You may also specify an alternate starting port for the replica set members:

.. code-block:: bash

    scons test --test-server=123.4.5.67 --seed-start-port=40000


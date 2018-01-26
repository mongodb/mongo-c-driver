:man_page: valgrind

VALGRIND
========

BSON can be allocated either on stack or heap depending on its size. Thus if the user forgets to destroy a BSON, it can be a potential memory leak.
This flag is used for compiling libbson in a way that the BSON always contains resources on heap.
Therefore, valgrind could catch all the possible BSON memory leaks.

The BSON_MEMCHECK flag is called ``-DBSON_MEMCHECK``. It is a C macro in the libbson codebase that forces the BSON to be allocated with a byte of malloc-ed buffer.

Building the libbson with memory check
--------------------------------------

You can use either autotools or cmake to build the libbson as long as you pass in the ``-DBSON_MEMCHECK`` cflag.
Then you can run the valgrind test via ``make valgrind``.

.. code-block:: none

   cd mongo-c-driver
   cmake -DCMAKE_C_FLAGS="-DBSON_MEMCHECK -g" .
   make
   MONGOC_TEST_VALGRIND=on
   valgrind --error-exitcode=1 --leak-check=full --track-origins=yes --gen-suppressions=all --num-callers=32 --suppressions=./valgrind.suppressions ./test-libmongoc


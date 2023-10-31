:man_page: bson_lifetimes

BSON lifetimes
==============
A :symbol:`bson_t` may contain its data directly or may contain pointers to heap-allocated memory. Overwriting an existing :symbol:`bson_t`
or allowing a stack-allocated BSON to go out of scope may cause a memory leak. A :symbol:`bson_t` should always be destroyed with
:symbol:`bson_destroy()`.

BSON out parameters
-------------------
A `bson_t*` used as an out parameter must point to valid overwritable storage for a new :symbol:`bson_t`.

This can be on the stack:
.. code-block:: c

  bson_t stack_doc = BSON_INITIALIZER;
  example_get_doc (&stack_doc);
  bson_destroy (&stack_doc);

Or on the heap:
.. code-block:: c

  bson_t \*heap_doc = bson_malloc (sizeof (bson_t));
  example_get_doc (heap_doc);
  bson_destroy (heap_doc);
  bson_free (heap_doc);

Omitting `bson_destroy` in either case may cause memory leaks.
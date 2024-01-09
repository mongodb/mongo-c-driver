:man_page: bson_lifetimes

:symbol:`bson_t` lifetimes
==========================
A :symbol:`bson_t` may contain its data directly or may contain pointers to heap-allocated memory. Overwriting an existing :symbol:`bson_t`
or allowing a stack-allocated :symbol:`bson_t` to go out of scope may cause a memory leak. A :symbol:`bson_t` should always be destroyed with
:symbol:`bson_destroy()`.

:symbol:`bson_t` out parameters
-------------------------------
A :symbol:`bson_t` pointer used as an out parameter must point to valid overwritable storage for a new :symbol:`bson_t` which must be one of:

#. Uninitialized storage for a :symbol:`bson_t`.
#. A zero-initialized :symbol:`bson_t` object.
#. A :symbol:`bson_t` object initialized with ``BSON_INITIALIZER``.
#. A :symbol:`bson_t` object not created with :symbol:`bson_new` that was destroyed with :symbol:`bson_destroy`.

This can be on the stack:

.. code-block:: c

  bson_t stack_doc = BSON_INITIALIZER;
  example_get_doc (&stack_doc);
  bson_destroy (&stack_doc);

Or on the heap:

.. code-block:: c

  bson_t *heap_doc = bson_malloc (sizeof (bson_t));
  example_get_doc (heap_doc);
  bson_destroy (heap_doc);
  bson_free (heap_doc);

Omitting :symbol:`bson_destroy` in either case may cause memory leaks.

.. warning::

  Passing a :symbol:`bson_t` pointer obtained from :symbol:`bson_new` as an out parameter will result in a leak of the :symbol:`bson_t` struct.

  .. code-block:: c

      bson_t *heap_doc = bson_new ();
      example_get_doc (heap_doc);
      bson_destroy (heap_doc); // Leaks the `bson_t` struct!
      
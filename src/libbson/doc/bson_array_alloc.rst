:man_page: bson_array_alloc

bson_array_alloc()
==================

Synopsis
--------

.. code-block:: c

  #define BSON_ARRAY_ALLOC(Count, Type) \
     (Type*) bson_array_alloc (Count, sizeof (Type))

  void *
  bson_array_alloc (size_t num_elems, size_t elem_size);

Parameters
----------

* ``num_elems``: A size_t containing the number of objects to allocate.
* ``elem_size``: A size_t containing the size of each object in bytes.

Description
-----------

This is a portable ``malloc()`` wrapper to allocate an array of objects.

If there was a failure to allocate ``num_elems * elem_size`` bytes, the process will be aborted.

.. warning::

  This function will abort on failure to allocate memory.

Returns
-------

A pointer to a memory region which *HAS NOT* been zeroed.
:man_page: bson_array_alloc0

bson_array_alloc0()
===================

Synopsis
--------

.. code-block:: c

  #define BSON_ARRAY_ALLOC0(Count, Type) \
     (Type*) bson_array_alloc0 (Count, sizeof (Type))

  void *
  bson_array_alloc0 (size_t num_elems, size_t elem_size);

Parameters
----------

* ``num_elems``: A size_t containing the number of objects to allocate.
* ``elem_size``: A size_t containing the size of each object in bytes.

Description
-----------

This is a portable ``calloc()`` wrapper to allocate an array of objects that also sets the memory to zero.

In general, this function will return an allocation at least ``sizeof(void*)`` bytes or bigger.

If there was a failure to allocate ``num_elems * elem_size`` bytes, the process will be aborted.

.. warning::

  This function will abort on failure to allocate memory.

Returns
-------

A pointer to a memory region which *HAS* been zeroed.
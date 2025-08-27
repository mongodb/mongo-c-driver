:man_page: bson_alloc_array0

bson_alloc_array0()
=============

Synopsis
--------

.. code-block:: c

  void *
  bson_alloc_array0 (size_t type_size, size_t num_elems);

Parameters
----------

* ``type_size``: A size_t containing the size in bytes of a single object in the array. 
* ``num_elems``: A size_t containing the number of objects to be stored in the array.

Description
-----------

This is a portable ``malloc()`` wrapper to allocate an array of objects that also sets the memory to zero.

In general, this function will return an allocation at least ``sizeof(void*)`` bytes or bigger.

If there was a failure to allocate ``type_size * num_elems`` bytes, the process will be aborted.

.. warning::

  This function will abort on failure to allocate memory.

Returns
-------

A pointer to a memory region which *HAS* been zeroed.
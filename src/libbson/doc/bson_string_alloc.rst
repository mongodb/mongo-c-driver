:man_page: bson_string_alloc

bson_string_alloc()
===================

Synopsis
--------

.. code-block:: c

  bson_string_t *
  bson_string_alloc (uint8_t len);

Parameters
----------

* ``len``: The length of the string to be allocated.

Description
-----------

Creates a new string builder, which uses power-of-two growth of buffers. The buffer is initialized to the given length and the required memory is allocated. Use the various bson_string_append*() functions to append to the string.

Returns
-------

A newly allocated bson_string_t that should be freed with bson_string_free() when no longer in use.


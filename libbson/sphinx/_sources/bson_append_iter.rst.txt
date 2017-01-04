:man_page: bson_append_iter

bson_append_iter()
==================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_iter (bson_t *bson,
                    const char *key,
                    int key_length,
                    const bson_iter_t *iter);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``iter``: A bson_iter_t.

Description
-----------

TODO:

Returns
-------

TODO:


:man_page: bson_iter_binary_equal

bson_iter_binary_equal()
========================

Synopsis
--------

.. code-block:: c

  bool
  bson_iter_binary_equal (const bson_iter_t *iter_a, const bson_iter_t *iter_b);

Parameters
----------

* ``iter_a``: A :symbol:`bson_iter_t`.
* ``iter_b``: A :symbol:`bson_iter_t`.

Description
-----------

Compare two BSON_TYPE_BINARY fields for exact equality.

This is the preferred way to compare :doc:`binary_vector` values for equality.

Returns
-------

``true`` if both iterators point to BSON_TYPE_BINARY fields with identical subtype and contents. ``false`` if there is any difference in subtype, length, or content, or if the fields are not binary type.
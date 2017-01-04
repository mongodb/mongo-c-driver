:man_page: bson_copy_to_excluding

bson_copy_to_excluding()
========================

Synopsis
--------

.. code-block:: c

  void
  bson_copy_to_excluding (const bson_t *src,
                          bson_t *dst,
                          const char *first_exclude,
                          ...) BSON_GNUC_NULL_TERMINATED;

Parameters
----------

* ``src``: A :symbol:`bson_t <bson_t>`.
* ``dst``: A :symbol:`bson_t <bson_t>`.
* ``first_exclude``: The first field name to exclude.

Description
-----------

The :symbol:`bson_copy_to_excluding() <bson_copy_to_excluding>` function shall copy all fields from ``src`` to ``dst`` except those speified by the variadic, NULL terminated list of keys starting from ``first_exclude``.

.. warning:

  This is generally not needed except in very special situations.


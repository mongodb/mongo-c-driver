:man_page: bson_copy_to_excluding

bson_copy_to_excluding()
========================

.. warning::
   .. deprecated:: 1.1.0

      Use :symbol:`bson_copy_to_excluding_noinit()` instead.

Synopsis
--------

.. code-block:: c

  void
  bson_copy_to_excluding (const bson_t *src,
                          bson_t *dst,
                          const char *first_exclude,
                          ...);

Parameters
----------

* ``src``: A :symbol:`bson_t`.
* ``dst``: A :symbol:`bson_t`.
* ``first_exclude``: The first field name to exclude.

Description
-----------

The :symbol:`bson_copy_to_excluding()` function shall copy all fields from
``src`` to ``dst`` except those specified by the variadic, NULL terminated list
of keys starting from ``first_exclude``.

.. warning::

  :symbol:`bson_init` is called on ``dst``.


:man_page: bson_value_copy

bson_value_copy()
=================

Synopsis
--------

.. code-block:: c

  void
  bson_value_copy (const bson_value_t *src, bson_value_t *dst);

Parameters
----------

* ``src``: A :symbol:`bson_value_t` to copy from.
* ``dst``: A :symbol:`bson_value_t` to copy into.

Description
-----------

This function will copy the boxed content in ``src`` into ``dst``. ``dst`` must be freed with :symbol:`bson_value_destroy()` when no longer in use.

.. note::

  If ``src`` represents a BSON UTF-8 string, :symbol:`bson_value_copy` attempts to NULL terminate the copied string in ``dst``. If ``src.value.v_utf8.len`` is `SIZE_MAX`, the copied string is (necessarily) not NULL terminated.

:man_page: bson_init_static

bson_init_static()
==================

Synopsis
--------

.. code-block:: c

  bool
  bson_init_static (bson_t *b, const uint8_t *data, size_t length);

Parameters
----------

* ``b``: A :symbol:`bson_t <bson_t>`.
* ``data``: A buffer to initialize with.
* ``length``: The length of ``data`` in bytes.

Description
-----------

The :symbol:`bson_init_static() <bson_init_static>` function shall shall initialize a read-only :symbol:`bson_t <bson_t>` on the stack using the data provided. No copies of the data will be made and therefore must remain valid for the lifetime of the :symbol:`bson_t <bson_t>`.

Returns
-------

:symbol:`bson_init_static() <bson_init_static>` will return ``true`` if the :symbol:`bson_t <bson_t>` was successfully initialized.


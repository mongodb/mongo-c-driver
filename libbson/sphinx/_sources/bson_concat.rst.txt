:man_page: bson_concat

bson_concat()
=============

Synopsis
--------

.. code-block:: c

  bool
  bson_concat (bson_t *dst, const bson_t *src);

Parameters
----------

* ``dst``: A :symbol:`bson_t <bson_t>`.
* ``src``: A :symbol:`bson_t <bson_t>`.

Description
-----------

The :symbol:`bson_concat() <bson_concat>` function shall append the contents of ``src`` to ``dst``.

Returns
-------

true if the operation was applied successfully, otherwise false and ``dst`` should be discarded.


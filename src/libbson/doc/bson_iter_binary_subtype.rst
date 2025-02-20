:man_page: bson_iter_binary_subtype

bson_iter_binary_subtype()
==========================

Synopsis
--------

.. code-block:: c

  bson_subtype_t
  bson_iter_binary_subtype (const bson_iter_t *iter);

Parameters
----------

* ``iter``: A :symbol:`bson_iter_t`.

Description
-----------

This function shall return the subtype of a BSON_TYPE_BINARY element. It is a programming error to call this function on a field that is not of type BSON_TYPE_BINARY. You can check this with the BSON_ITER_HOLDS_BINARY() macro or :symbol:`bson_iter_type()`.

Equivalent to the ``subtype`` output parameter of :symbol:`bson_iter_binary`.

:man_page: bson_iter_next

bson_iter_next()
================

Synopsis
--------

.. code-block:: c

  bool
  bson_iter_next (bson_iter_t *iter);

Parameters
----------

* ``iter``: A :symbol:`bson_iter_t`.

Description
-----------

Advances ``iter`` to the next element in the document.

This function does not fully validate BSON. Embedded documents and arrays are
checked for buffer bounds, but not for the other requirements checked by
:symbol:`bson_init_static` or :symbol:`bson_validate`.

Returns
-------

true if ``iter`` was advanced. Returns false if ``iter`` has passed the last element in the document or detected invalid BSON.

It is a programming error to use ``iter`` after this function has returned false.

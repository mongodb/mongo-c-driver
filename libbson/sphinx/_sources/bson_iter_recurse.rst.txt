:man_page: bson_iter_recurse

bson_iter_recurse()
===================

Synopsis
--------

.. code-block:: c

  bool
  bson_iter_recurse (const bson_iter_t *iter, bson_iter_t *child);

Parameters
----------

* ``iter``: A :symbol:`bson_iter_t <bson_iter_t>`.
* ``child``: A :symbol:`bson_iter_t <bson_iter_t>`.

Description
-----------

The :symbol:`bson_iter_recurse() <bson_iter_recurse>` function shall initialize ``child`` using the embedded document or array currently observed by ``iter``.

If there was a failure to initialize the ``iter``, false is returned and both ``iter`` and ``child`` should be considered invalid.

This should only be called when observing an element of type BSON_TYPE_ARRAY or BSON_TYPE_DOCUMENT.

Returns
-------

true if ``child`` has been initialized. Otherwise, ``child`` should be considered invalid.


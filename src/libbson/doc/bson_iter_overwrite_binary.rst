:man_page: bson_iter_overwrite_binary

bson_iter_overwrite_binary()
============================

Synopsis
--------

.. code-block:: c

  void
  bson_iter_overwrite_binary (bson_iter_t *iter,
                              bson_subtype_t subtype,
                              uint32_t *binary_len,
                              uint8_t **binary);

Parameters
----------

* ``iter``: A :symbol:`bson_iter_t`.
* ``subtype``: The expected :symbol:`bson_subtype_t`.
* ``binary_len``: A location for the length of ``binary``.
* ``binary``: A location for a pointer to the mutable buffer.

Description
-----------

The ``bson_iter_overwrite_binary()`` function obtains mutable access to a BSON_TYPE_BINARY element in place.

This may only be done when the underlying bson document allows mutation.

It is a programming error to call this function when ``iter`` is not observing an element of type BSON_TYPE_BINARY and the provided ``subtype``.

The buffer that ``binary`` points to is only valid until the iterator's :symbol:`bson_t` is otherwise modified or freed.

.. seealso::

  | :symbol:`bson_iter_binary`

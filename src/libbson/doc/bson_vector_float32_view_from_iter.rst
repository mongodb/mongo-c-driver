:man_page: bson_vector_float32_view_from_iter

bson_vector_float32_view_from_iter()
====================================

Initialize a :symbol:`bson_vector_float32_view_t` from a :symbol:`bson_iter_t` pointing to a valid Vector of ``float32`` element type.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_float32_view_from_iter (bson_vector_float32_view_t *view_out,
                                      bson_iter_t *iter);

Parameters
----------

* ``view_out``: A :symbol:`bson_vector_float32_view_t` is written here on success.
* ``iter``: A valid :symbol:`bson_iter_t`.

Description
-----------

The provided iterator, which must point to some kind of BSON item, will be checked for a valid Vector of ``float32`` element type.
On success, a :symbol:`bson_vector_float32_view_t` is set to point to the same underlying :symbol:`bson_t` buffer as the provided :symbol:`bson_iter_t`.
The view will only be valid until the containing document is destroyed or otherwise modified.

Returns
-------

Returns true if the view was successfully initialized.

.. seealso::

  | :symbol:`bson_vector_float32_const_view_from_iter`

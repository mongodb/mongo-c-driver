:man_page: bson_error_clear

bson_error_clear()
==================

Synopsis
--------

.. code-block:: c

  void
  bson_error_clear (bson_error_t *error);

Parameters
----------

* ``error``: A pointer to storage for a :symbol:`bson_error_t`, or NULL.

Description
-----------

If given a non-null pointer to a :symbol:`bson_error_t`, this function will
clear any error value that is stored in the pointed-to object. If given a null
pointer, this function has no effect.

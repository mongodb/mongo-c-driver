:man_page: bson_error_reset

bson_error_reset()
==================

Synopsis
--------

.. code-block:: c

  #define bson_error_reset(ErrorPointer)

Parameters
----------

* ``ErrorPointer``: An l-value expression of type ``bson_error_t*``. May be a
  null pointer.

Description
-----------

This function-like macro modifies a pointer to :symbol:`bson_error_t` to be
non-null, and clears any contained value using :symbol:`bson_error_clear`.

If the given pointer object is null, then the pointer is updated to point to a
local anonymous :symbol:`bson_error_t` object. After the evaluation of this
macro, it is guaranteed that the given pointer is non-null.

.. important:: This function-like macro is not valid in C++!

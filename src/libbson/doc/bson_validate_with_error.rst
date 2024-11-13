:man_page: bson_validate_with_error

bson_validate_with_error()
==========================

Synopsis
--------

.. code-block:: c

  bool
  bson_validate_with_error (const bson_t *bson,
                            bson_validate_flags_t flags,
                            bson_error_t *error);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``flags``: A bitwise-or of all desired :symbol:`bson_validate_flags_t`.
* ``error``: Optional :symbol:`bson_error_t`.

Description
-----------

Validates a BSON document by walking through the document and inspecting the keys and values for valid content.

You can modify how the validation occurs through the use of the ``flags`` parameter, see :symbol:`bson_validate_flags_t` for details.

Returns
-------

If ``bson`` passes the requested validations, returns true.
Otherwise, returns false and if ``error`` is non-`NULL` it will be filled out with details.

The :symbol:`bson_error_t` domain is set to ``BSON_ERROR_INVALID``. Its code is set to one of the ``bson_validate_flags_t`` flags indicating which validation failed; for example, if a key contains invalid UTF-8, then the code is set to ``BSON_VALIDATE_UTF8``, but if the basic structure of the BSON document is corrupt, the code is set to ``BSON_VALIDATE_NONE``. The error message is filled out, and gives more detail if possible.

To get the specific location of the error, use :symbol:`bson_validate_with_error_and_offset()` instead.

.. seealso::

  | :symbol:`bson_validate()`, :symbol:`bson_validate_with_error_and_offset()`.

  | :symbol:`bson_visitor_t` can be used for custom validation, :ref:`example_custom_validation`.

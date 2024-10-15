:man_page: bson_validate_with_error_and_offset

bson_validate_with_error_and_offset()
=====================================

Synopsis
--------

.. code-block:: c

  bool
  bson_validate_with_error_and_offset (const bson_t *bson,
                                       bson_validate_flags_t flags,
                                       size_t *offset,
                                       bson_error_t *error)

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``flags``: A bitwise-or of all desired :symbol:`bson_validate_flags_t`.
* ``offset``: Optional location where the error offset will be written.
* ``error``: Optional :symbol:`bson_error_t`.

Description
-----------

Validates a BSON document by walking through the document and inspecting the keys and values for valid content.

You can modify how the validation occurs through the use of the ``flags`` parameter, see :symbol:`bson_validate_flags_t` for details.

Returns
-------

If ``bson`` passes the requested validations, returns true.
Otherwise, returns false and writes each non-`NULL` output parameter: ``offset`` with the byte offset of the detected error and ``error`` with the details.

The :symbol:`bson_error_t` domain is set to ``BSON_ERROR_INVALID``. Its code is set to one of the ``bson_validate_flags_t`` flags indicating which validation failed; for example, if a key contains invalid UTF-8, then the code is set to ``BSON_VALIDATE_UTF8``, but if the basic structure of the BSON document is corrupt, the code is set to ``BSON_VALIDATE_NONE``. The error message is filled out, and gives more detail if possible.

.. seealso::

  | :symbol:`bson_validate()`, :symbol:`bson_validate_with_error()`.

  | :symbol:`bson_visitor_t` can be used for custom validation, :ref:`example_custom_validation`.

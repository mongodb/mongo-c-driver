:man_page: bson_validate

bson_validate()
===============

Synopsis
--------

.. code-block:: c

  bool
  bson_validate (const bson_t *bson, bson_validate_flags_t flags, size_t *offset);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``flags``: A bitwise-or of all desired :symbol:`bson_validate_flags_t`.
* ``offset``: Optional location where the error offset will be written.

Description
-----------

Validates a BSON document by walking through the document and inspecting the keys and values for valid content.

You can modify how the validation occurs through the use of the ``flags`` parameter, see :symbol:`bson_validate_flags_t` for details.

Returns
-------

If ``bson`` passes the requested validations, returns true.
Otherwise, returns false and if ``offset`` is non-`NULL` it will be written with the byte offset in the document where an error was detected.

To get more information about the specific validation failure, use :symbol:`bson_validate_with_error_and_offset()` instead.

.. seealso::

  | :symbol:`bson_validate_with_error()`, :symbol:`bson_validate_with_error_and_offset()`.

  | :symbol:`bson_visitor_t` can be used for custom validation, :ref:`example_custom_validation`.

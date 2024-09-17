:man_page: bson_string_append_codepoint

bson_string_append_codepoint()
=======================

Synopsis
--------

.. code-block:: c

  void
  bson_string_append_codepoint (bson_string_t *string, bson_unichar_t unichar);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``unichar``: A :symbol:`bson_unichar_t` to append to the string.

Description
-----------

Appends the Unicode codepoint ``unichar`` to ``string`` as a Unicode escape sequence (e.g., \uXXXX).

:man_page: bson_string_truncate

bson_string_truncate()
======================

.. warning::
   .. deprecated:: 1.29.0


Synopsis
--------

.. code-block:: c

  void
  bson_string_truncate (bson_string_t *string, uint32_t len);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``len``: The new length of the string, excluding the trailing ``\0``.

Description
-----------

Truncates the string so that it is ``len`` bytes in length. This must be smaller or equal to the current length of the string.

A ``\0`` byte will be placed where the end of the string occurs.

.. warning:: This function is oblivious to UTF-8 structure. If truncation occurs in the middle of a UTF-8 byte sequence, the resulting :symbol:`bson_string_t` will contain invalid UTF-8.

.. warning:: The length of the resulting string (including the ``NULL`` terminator) MUST NOT exceed ``UINT32_MAX``.

:man_page: bson_string_append_c

bson_string_append_c()
======================

Synopsis
--------

.. code-block:: c

  void
  bson_string_append_c (bson_string_t *string, char str);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``str``: An ASCII char.

Description
-----------

Appends ``c`` to the string builder ``string``.

.. warning:: This function will abort if the length of the resulting string (including the NULL terminator) would exceed ``UINT32_MAX``.

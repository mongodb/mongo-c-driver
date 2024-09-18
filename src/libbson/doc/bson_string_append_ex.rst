:man_page: bson_string_append_ex

bson_string_append_ex()
=======================

Synopsis
--------

.. code-block:: c

  void
  bson_string_append_ex (bson_string_t *string, const char *str, const size_t len);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``str``: A string.
* ``len``: The length of ``str`` in bytes.

Description
-----------

Copies ``len`` bytes from ``str`` to ``string``. This allows appending strings containing NULLs.


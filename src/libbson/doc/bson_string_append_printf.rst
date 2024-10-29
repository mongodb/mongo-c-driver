:man_page: bson_string_append_printf

bson_string_append_printf()
===========================

.. warning::
   .. deprecated:: 1.29.0

Synopsis
--------

.. code-block:: c

  void
  bson_string_append_printf (bson_string_t *string, const char *format, ...);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``format``: A printf style format string.

Description
-----------

Like bson_string_append() but formats a printf style string and then appends that to ``string``.

.. warning:: The length of the resulting string (including the ``NULL`` terminator) MUST NOT exceed ``UINT32_MAX``.

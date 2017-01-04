:man_page: bson_uint32_to_string

bson_uint32_to_string()
=======================

Synopsis
--------

.. code-block:: c

  size_t
  bson_uint32_to_string (uint32_t value,
                         const char **strptr,
                         char *str,
                         size_t size);

See :ref:`Array Element Key Building <performance_array_element_key_building>` for example usage.

Parameters
----------

* ``value``: A uint32_t.
* ``strptr``: A location for the resulting string pointer.
* ``str``: A location to buffer the string.
* ``size``: A size_t containing the size of ``str``.

Description
-----------

Converts ``value`` to a string.

If ``value`` is from 0 to 999, it will use a constant string in the data section of the library.

If not, a string will be formatted using ``str`` and ``snprintf()``.

``strptr`` will always be set. It will either point to ``str`` or a constant string. Use this as your key.

Returns
-------

The number of bytes in the resulting string.


:man_page: bson_new_from_json

bson_new_from_json()
====================

Synopsis
--------

.. code-block:: c

  bson_t *
  bson_new_from_json (const uint8_t *data, ssize_t len, bson_error_t *error);

Parameters
----------

* ``data``: A UTF-8 encoded string containing valid JSON.
* ``len``: The length of ``data`` in bytes excluding a trailing ``\0`` or -1 to determine the length with ``strlen()``.
* ``error``: An optional location for a :symbol:`bson_error_t`.

Description
-----------

The ``bson_new_from_json()`` function allocates and initializes a new :symbol:`bson_t` by parsing the JSON found in ``data``.
Only a single JSON object may exist in ``data`` or an error will be set and NULL returned. 

Deprecated behavior: If there are multiple comma-separated JSONs in ``data``, the keys from all JSONs are merged in the returned BSON.
For example, ``{"a": 1},{"b": 2}`` is parsed as ``{"a": 1, "b": 2}``. If the first character encountered after the last valid
JSON object is ``{``, all following characters are ignored and no error is set.
Otherwise, an error will be set and NULL returned. 
Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

A newly allocated :symbol:`bson_t` if successful, otherwise NULL and ``error`` is set.

.. only:: html

  .. include:: includes/seealso/create-bson.txt
  .. include:: includes/seealso/json.txt

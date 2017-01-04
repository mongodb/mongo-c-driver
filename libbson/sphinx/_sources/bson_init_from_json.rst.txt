:man_page: bson_init_from_json

bson_init_from_json()
=====================

Synopsis
--------

.. code-block:: c

  bool
  bson_init_from_json (bson_t *bson,
                       const char *data,
                       ssize_t len,
                       bson_error_t *error);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``data``: A UTF-8 encoded string containing valid JSON.
* ``len``: The length of ``data`` in bytes excluding a trailing ``\0`` or -1 to determine the length with ``strlen()``.
* ``error``: An optional location for a :symbol:`bson_error_t <bson_error_t>`.

Description
-----------

The ``bson_init_from_json()`` function will initialize a new :symbol:`bson_t <bson_t>` by parsing the JSON found in ``data``. Only a single JSON object may exist in ``data`` or an error will be set and false returned.

``data`` should be in `MongoDB Extended JSON <http://docs.mongodb.org/manual/reference/mongodb-extended-json/>`_ format.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

true if successful, otherwise false and ``error`` is set.


:man_page: bson_append_timeval

bson_append_timeval()
=====================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_timeval (bson_t *bson,
                       const char *key,
                       int key_length,
                       struct timeval *value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: A struct timeval.

Description
-----------

The :symbol:`bson_append_timeval() <bson_append_timeval>` function is a helper that takes a ``struct timeval`` instead of milliseconds since the UNIX epoch.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.


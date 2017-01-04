:man_page: bson_append_date_time

bson_append_date_time()
=======================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_date_time (bson_t *bson,
                         const char *key,
                         int key_length,
                         int64_t value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: The date and time as specified in milliseconds since the UNIX epoch.

Description
-----------

The :symbol:`bson_append_date_time() <bson_append_date_time>` function shall append a new element to a ``bson`` document containing a date and time with no timezone information. ``value`` is assumed to be in UTC format of milliseconds since the UNIX epoch. ``value`` *MAY* be negative.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.


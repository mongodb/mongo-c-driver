:man_page: mongoc_uri_get_string

mongoc_uri_get_string()
=======================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_uri_get_string (const mongoc_uri_t *uri);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.

Description
-----------

Fetches the URI as a string.

.. warning::

   This function returns the original, unredacted connection string, which may
   contain usernames, passwords, and sensitive option arguments. Do not include
   its result in logs or diagnostic messages.

Returns
-------

Returns a string which should not be modified or freed.

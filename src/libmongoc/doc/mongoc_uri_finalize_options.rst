:man_page: mongoc_uri_finalize_options

mongoc_uri_finalize_options()
=============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_uri_finalize_options (mongoc_uri_t *uri);
                               bson_error_t *error);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

Finalizes URI options and checks for invalid combinations of options.

This function should be called after setting individual options on a URI and
before using the URI to construct a client or client pool. It is not necessary
to call this function if additional options have not been set on the URI after
its construction from a string (e.g. :symbol:`mongoc_uri_new_with_error`).

Returns
-------

Returns true if ``uri`` is successfully finalized and valid; otherwise false and
populates ``error`` with the error description.

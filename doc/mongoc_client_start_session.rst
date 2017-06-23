:man_page: mongoc_client_start_session

mongoc_client_start_session()
=============================

Synopsis
--------

.. code-block:: c

  mongoc_session_t *
  mongoc_client_start_session (mongoc_client_t *client,
                               mongoc_session_opt_t *opts,
                               bson_error_t *error)

.. include:: includes/session-lifecycle.txt

Calling :symbol:`mongoc_client_start_session()` creates a server-side session only if the session is configured with options, such as retryable writes, that require a server session.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``error``: A :symbol:`bson:bson_error_t`.

Returns
-------

If successful, this function returns a newly allocated :symbol:`mongoc_session_t` that should be freed with :symbol:`mongoc_session_destroy()` when no longer in use. On error, returns NULL and sets ``error``.

Errors
------

This function can fail if ``opts`` is misconfigured, if the MongoDB server is unavailable, if the session is configured with options that the server does not support, or if there was server error when the driver attempted to start the server session.

.. only:: html

  .. taglist:: See Also:
    :tags: session

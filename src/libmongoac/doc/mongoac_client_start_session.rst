:man_page: mongoac_client_start_session

mongoac_client_start_session()
==============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_client_session_t *mongoac_client_start_session(const mongoac_client_t *client,
                                                          const mongoac_session_options_t *options,
                                                          mongoac_error_t *error);

Description
-----------

Starts a new :symbol:`mongoac_client_session_t` via `Client::start_session() <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#method.start_session>`_.

Must be destroyed with :symbol:`mongoac_client_session_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``client`` is null.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_client_start_session_async()`
   | :symbol:`mongoac_client_session_destroy`

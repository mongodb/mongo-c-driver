:man_page: mongoac_client_new

mongoac_client_new()
====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_client_t *mongoac_client_new(const mongoac_client_options_t *options,
                                        mongoac_error_t *error);

Description
-----------

Create a new :symbol:`mongoac_client_t` with the given `ClientOptions <https://docs.rs/mongodb/latest/mongodb/options/struct.ClientOptions.html>`_.

When ``options`` is null, the client is created using a default `ClientOptions <https://docs.rs/mongodb/latest/mongodb/options/struct.ClientOptions.html>`_.

Must be destroyed with :symbol:`mongoac_client_destroy()`.

Errors
------

This function may return the following errors:

- Runtime Error (mongoac): failed to create the associated runtime.
- Options Error (rust): ``mongodb::Client::with_options()`` failed to create the client with the given options.

.. seealso::

   | :symbol:`mongoac_client_clone`
   | :symbol:`mongoac_client_destroy`
   | :symbol:`mongoac_client_new_with_uri_str`
   | :symbol:`mongoac_error_t`

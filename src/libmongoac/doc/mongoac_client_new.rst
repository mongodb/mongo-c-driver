:man_page: mongoac_client_new

mongoac_client_new()
====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_client_t *mongoac_client_new(mongoac_string_view_t conn_str, mongoac_error_t *error);

Description
-----------

Create a new :symbol:`mongoac_client_t` connected to a MongoDB cluster `as specified by <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#method.with_uri_str>`_ ``conn_str``.

Must be destroyed with :symbol:`mongoac_client_destroy()`.

The given MongoDB connection string is parsed via `mongodb::ClientOptions::parse() <https://docs.rs/mongodb/latest/mongodb/options/struct.ClientOptions.html#method.parse>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``conn_str`` is null or invalid UTF-8.
- Runtime Error (mongoac): failed to create the associated runtime.
- Parsing Error (rust): ``mongodb::ClientOptions::parse()`` failed to parse the connection string.
- Options Error (rust): ``mongodb::Client::with_options()`` failed to create the client with the given options.

.. seealso::

   | :symbol:`mongoac_client_clone`
   | :symbol:`mongoac_client_destroy`
   | :symbol:`mongoac_error_t`

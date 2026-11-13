:man_page: mongoac_client_new_with_uri_str_async

mongoac_client_new_with_uri_str_async()
=======================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_future_t *mongoac_client_new_with_uri_str_async(mongoac_string_view_t uri,
                                                           mongoac_error_t *error);

Description
-----------

Async equivalent to :symbol:`mongoac_client_new_with_uri_str()`.

When ``uri`` is null or invalid UTF-8, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

Errors
------

In addition to errors produced by the async operation, this function may also return the following errors:

- Runtime Error (mongoac): failed to create the associated runtime.

.. seealso::

   | :symbol:`mongoac_client_new_with_uri_str`
   | :symbol:`mongoac_future_get_client`
   | :symbol:`mongoac_future_t`

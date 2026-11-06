:man_page: mongoac_client_start_session_async

mongoac_client_start_session_async()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_future_t *mongoac_client_start_session_async(const mongoac_client_t *client,
                                                        const mongoac_session_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_client_start_session()`.

When ``client`` is null, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_client_start_session()`
   | :symbol:`mongoac_future_get_client_session`

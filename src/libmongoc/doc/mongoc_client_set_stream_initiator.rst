:man_page: mongoc_client_set_stream_initiator

mongoc_client_set_stream_initiator()
====================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_set_stream_initiator (mongoc_client_t *client,
                                      mongoc_stream_initiator_t initiator,
                                      void *user_data);

The :symbol:`mongoc_client_set_stream_initiator()` function shall associate a given :symbol:`mongoc_client_t` with a new stream initiator. This will completely replace the default transport (buffered TCP, possibly with TLS). The ``initiator`` should fulfill the :symbol:`mongoc_stream_t` contract. ``user_data`` is passed through to the ``initiator`` callback and may be used for whatever run time customization is necessary.

It is a programming error to call this function on a :symbol:`mongoc_client_t` from a :symbol:`mongoc_client_pool_t`.

.. versionchanged:: 2.0.0 This function logs an error and immediately returns if ``client`` is from a :symbol:`mongoc_client_pool_t`. Previously this function unsafely applied the initiator to the pooled client.

If ``user_data`` is passed, it is the application's responsibility to ensure ``user_data`` remains valid for the lifetime of the client.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``initiator``: A :symbol:`mongoc_stream_initiator_t <mongoc_client_t>`.
* ``user_data``: User supplied pointer for callback function.


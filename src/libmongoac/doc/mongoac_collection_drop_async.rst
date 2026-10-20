:man_page: mongoac_collection_drop_async

mongoac_collection_drop_async()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_drop_async(const mongoac_collection_t *coll,
                                                   mongoac_client_session_t *session,
                                                   const mongoac_drop_collection_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_drop()`.

When ``coll`` is null, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`

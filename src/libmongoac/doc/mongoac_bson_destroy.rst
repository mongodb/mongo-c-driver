:man_page: mongoac_bson_destroy

mongoac_bson_destroy()
======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/bson.h>

   void mongoac_bson_destroy(mongoac_bson_t bson);

Description
-----------

Destroys the given :symbol:`mongoac_bson_t`.

When ``bson.ptr`` is null, does nothing.

.. important::

  When not null, ``bson.ptr`` and ``bson.len`` MUST equal the original values that were returned by the mongoac
  library when the BSON document was allocated.

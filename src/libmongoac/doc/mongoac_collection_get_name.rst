:man_page: mongoac_collection_get_name

mongoac_collection_get_name()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_string_view_t mongoac_collection_get_name(const mongoac_collection_t *coll);

Description
-----------

Returns the name of the given :symbol:`mongoac_collection_t`.

When ``coll`` is null, returns a null string.

.. important::

   The returned string is owned by the collection object and must not be modified or freed.

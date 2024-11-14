:man_page: mongoc_delete_flags_t

mongoc_delete_flags_t
=====================

.. warning::
   .. deprecated:: 1.9.0

      Use :symbol:`mongoc_collection_delete_one()` or :symbol:`mongoc_collection_delete_many()` instead.

Synopsis
--------

.. code-block:: c

  typedef enum {
     MONGOC_DELETE_NONE = 0,
     MONGOC_DELETE_SINGLE_REMOVE = 1 << 0,
  } mongoc_delete_flags_t;

Flags for deletion operations

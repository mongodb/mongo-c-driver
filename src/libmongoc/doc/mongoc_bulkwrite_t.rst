:man_page: mongoc_bulkwrite_t

mongoc_bulkwrite_t
==================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_bulkwrite_t mongoc_bulkwrite_t;

Description
-----------

:symbol:`mongoc_bulkwrite_t` provides an abstraction for submitting multiple write operations as a single batch.

After adding all of the write operations to the :symbol:`mongoc_bulkwrite_t`, call :symbol:`mongoc_bulkwrite_execute()`
to execute the operation.

.. warning::

  It is only valid to call :symbol:`mongoc_bulkwrite_execute()` once. The :symbol:`mongoc_bulkwrite_t` must be destroyed
  afterwards.

.. note::

  .. include:: includes/bulkwrite-vs-bulk_operation.txt

.. only:: html

  API
  ---

  .. toctree::
    :titlesonly:
    :maxdepth: 1


    mongoc_bulkwrite_insertoneopts_t
    mongoc_bulkwrite_append_insertone
    mongoc_bulkwrite_updateoneopts_t
    mongoc_bulkwrite_append_updateone
    mongoc_bulkwrite_updatemanyopts_t
    mongoc_bulkwrite_append_updatemany
    mongoc_bulkwrite_replaceoneopts_t
    mongoc_bulkwrite_append_replaceone
    mongoc_bulkwrite_deleteoneopts_t
    mongoc_bulkwrite_append_deleteone
    mongoc_bulkwrite_deletemanyopts_t
    mongoc_bulkwrite_append_deletemany
    mongoc_bulkwritereturn_t
    mongoc_bulkwrite_set_session
    mongoc_bulkwrite_execute
    mongoc_bulkwrite_destroy

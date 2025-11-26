:man_page: mongoc_bulkwrite_check_acknowledged_t

mongoc_bulkwrite_check_acknowledged_t
=====================================

Synopsis
--------

.. code-block:: c

   typedef struct {
      bool is_ok;           // true if no error
      bool is_acknowledged; // true if the previous call to `mongoc_bulkwrite_execute` used an acknowledged write concern
   } mongoc_bulkwrite_check_acknowledged_t;

Description
-----------

:symbol:`mongoc_bulkwrite_check_acknowledged_t` is returned by :symbol:`mongoc_bulkwrite_check_acknowledged`.

``is_ok`` is ``false`` if there was no previous call to :symbol:`mongoc_bulkwrite_execute` or if execution failed before
the write concern could be determined.

``is_acknowledged`` is ``true`` if the previous :symbol:`mongoc_bulkwrite_execute` call used an acknowledged write
concern.

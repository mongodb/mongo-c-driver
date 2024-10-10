:man_page: mongoc_bulkwritereturn_t

mongoc_bulkwritereturn_t
========================

Synopsis
--------

.. code-block:: c

   typedef struct {
     mongoc_bulkwriteresult_t *res;    // NULL if no known successful writes or write was unacknowledged.
     mongoc_bulkwriteexception_t *exc; // NULL if no error.
   } mongoc_bulkwritereturn_t;

Description
-----------

:symbol:`mongoc_bulkwritereturn_t` is returned by :symbol:`mongoc_bulkwrite_execute`.

``res`` or ``exc`` may outlive the :symbol:`mongoc_bulkwrite_t` that was executed.

``res`` is NULL if the :symbol:`mongoc_bulkwrite_t` has no known successful writes or was executed with an unacknowledged write concern.

``res`` must be freed with :symbol:`mongoc_bulkwriteresult_destroy`.

``exc`` is NULL if no error occurred.

``exc`` must be freed with :symbol:`mongoc_bulkwriteexception_destroy`.


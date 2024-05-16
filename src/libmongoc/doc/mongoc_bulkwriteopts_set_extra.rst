:man_page: mongoc_bulkwriteopts_set_extra

mongoc_bulkwriteopts_set_extra()
================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwriteopts_set_extra (mongoc_bulkwriteopts_t *self, const bson_t *extra);

Description
-----------

Appends all fields in ``extra`` to the outgoing ``bulkWrite`` command. Intended to support future server options. Prefer
other ``mongoc_bulkwriteopts_set_*`` helpers to avoid unexpected option conflicts.

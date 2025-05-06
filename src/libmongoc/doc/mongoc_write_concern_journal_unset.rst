:man_page: mongoc_write_concern_journal_unset

mongoc_write_concern_journal_unset()
====================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_write_concern_journal_unset (mongoc_write_concern_t *write_concern);

Parameters
----------

* ``write_concern``: Non-null pointer :symbol:`mongoc_write_concern_t` to be
  modified.

Description
-----------

Removes any prior modified to the ``journal`` attribute on a write concern
object. Returns ``true`` if-and-only-if the ``journal`` attribute of
``write_concern`` was set on the object.

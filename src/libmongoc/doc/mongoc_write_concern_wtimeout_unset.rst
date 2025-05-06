:man_page: mongoc_write_concern_wtimeout_unset

mongoc_write_concern_wtimeout_unset()
=====================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_write_concern_wtimeout_unset (mongoc_write_concern_t *write_concern);

Parameters
----------

* ``write_concern``: A :symbol:`mongoc_write_concern_t` to be modified

Description
-----------

Clears the ``wtimeout`` attribute of a write concern object. Returns ``true``
if-and-only-if the ``wtimeout`` attribute was previously set on the object.

:man_page: mongoc_write_concern_w_unset

mongoc_write_concern_w_unset()
==============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_write_concern_w_unset (mongoc_write_concern_t *write_concern);

Parameters
----------

* ``write_concern``: A :symbol:`mongoc_write_concern_t` to be modified.

Description
-----------

Clears the ``w`` and ``wtag`` values for the write concern. Returns ``true``
if-and-only-if either ``w`` or the ``wtag`` attributes were previously set on
the object.

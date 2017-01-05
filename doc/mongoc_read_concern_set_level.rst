:man_page: mongoc_read_concern_set_level

mongoc_read_concern_set_level()
===============================

Synopsis
--------

.. code-block:: none

  void
  mongoc_read_concern_set_level (mongoc_read_concern_t *read_concern,
                                 const char            *level);

Parameters
----------

* ``read_concern``: A :symbol:`mongoc_read_concern_t <mongoc_read_concern_t>`.
* ``level``: The readConcern level to use. Should be one of the :symbol:`MONGOC_READ_CONCERN_LEVEL_* <mongoc_read_concern_t#levels>` macros.

Description
-----------

The readConcern option allows clients to choose a level of isolation for their reads.


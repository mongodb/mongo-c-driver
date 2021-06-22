:man_page: mongoc_session_opts_set_snapshot

mongoc_session_opts_set_snapshot()
============================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_session_opts_set_snapshot (mongoc_session_opt_t *opts,
                                              bool snapshot);

Configure snapshot reads in a session. If true (false by default), each read operation in the session will be read from the same snapshot. Set to false to disable snapshot reads. See `the MongoDB Manual Entry for Snapshots <http://dochub.mongodb.org/core/snapshots>`_.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``snapshot``: True or false.

Example
-------

.. code-block:: c

   TODO CDRIVER-XXXX: give example of mongoc_session_opts_set_snapshot()

.. only:: html

  .. include:: includes/seealso/session.txt

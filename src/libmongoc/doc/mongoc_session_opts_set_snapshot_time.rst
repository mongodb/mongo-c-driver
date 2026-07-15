:man_page: mongoc_session_opts_set_snapshot_time

mongoc_session_opts_set_snapshot_time()
========================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_session_opts_set_snapshot_time (mongoc_session_opt_t *opts,
                                         uint32_t timestamp,
                                         uint32_t increment);

Configure the desired snapshot time for a snapshot session, expressed as a BSON Timestamp with timestamp and increment components. When set, the session will use this snapshot time instead of determining one from the first read operation. Typically the value is obtained from :symbol:`mongoc_client_session_get_snapshot_time()` on a previous session.

It is an error to call :symbol:`mongoc_client_start_session()` with a ``snapshotTime`` set unless :symbol:`mongoc_session_opts_set_snapshot()` is also set to true.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``timestamp``: A ``uint32_t`` for the timestamp component.
* ``increment``: A ``uint32_t`` for the increment component.

.. only:: html

  .. include:: includes/seealso/session.txt

:man_page: mongoc_client_session_get_snapshot_time

mongoc_client_session_get_snapshot_time()
==========================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_session_get_snapshot_time (const mongoc_client_session_t *session,
                                           uint32_t *timestamp,
                                           uint32_t *increment,
                                           bson_error_t *error);

Get the session's snapshot time, expressed as a BSON Timestamp with timestamp and increment components. The snapshot time is either the value passed to :symbol:`mongoc_session_opts_set_snapshot_time()` when the session was started, or the ``atClusterTime`` returned by the server on the session's first read operation ("find", "aggregate" or "distinct").

Calling this function on a session that was not started with :symbol:`mongoc_session_opts_set_snapshot()` set to true is an error. If the session is a snapshot session but no snapshot time has been established yet (no read operation has completed and no snapshot time was configured), this function returns false without setting ``error``.

Parameters
----------

* ``session``: A :symbol:`mongoc_client_session_t`.
* ``timestamp``: A pointer to a ``uint32_t`` to receive the timestamp component.
* ``increment``: A pointer to a ``uint32_t`` to receive the increment component.
* ``error``: An optional location for a :symbol:`bson_error_t`, or ``NULL``.

Returns
-------

Returns true and populates ``timestamp`` and ``increment`` if the snapshot time is available. Returns false otherwise, setting ``error`` if the session is not a snapshot session.

.. only:: html

  .. include:: includes/seealso/session.txt

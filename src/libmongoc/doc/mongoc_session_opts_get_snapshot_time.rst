:man_page: mongoc_session_opts_get_snapshot_time

mongoc_session_opts_get_snapshot_time()
========================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_session_opts_get_snapshot_time (const mongoc_session_opt_t *opts,
                                         uint32_t *timestamp,
                                         uint32_t *increment);

Get the snapshot time previously configured with :symbol:`mongoc_session_opts_set_snapshot_time()`. Returns false and leaves ``timestamp`` and ``increment`` unmodified if no snapshot time has been configured.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``timestamp``: A pointer to a ``uint32_t`` to receive the timestamp component.
* ``increment``: A pointer to a ``uint32_t`` to receive the increment component.

.. only:: html

  .. include:: includes/seealso/session.txt

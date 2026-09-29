:man_page: mongoc_set_thread_backend

mongoc_set_thread_backend()
===========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_set_thread_backend (const bson_thread_backend_t *backend);

Description
-----------

Installs a table of thread and synchronization callbacks for both libmongoc
and libbson. Call this function from a single thread before :symbol:`mongoc_init`
and before any other driver or libbson operation. The table is copied; callback
code and its supporting runtime must remain available through
:symbol:`mongoc_cleanup`.

Pass ``NULL`` to restore native operations before initialization. A backend
must provide compatible mutex and condition callbacks and matching create/join
and init/destroy callbacks. See the libbson documentation for ``bson_thread_backend_t`` and
``docs/dev/thread-backend.rst`` for the full callback contract.

Returns
-------

Returns ``true`` when configured, or ``false`` if :symbol:`mongoc_init` was
already called.

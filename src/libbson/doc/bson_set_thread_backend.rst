:man_page: bson_set_thread_backend

bson_set_thread_backend()
=========================

Synopsis
--------

.. code-block:: c

  void
  bson_set_thread_backend (const bson_thread_backend_t *backend);

Description
-----------

Installs a table of thread and synchronization callbacks for standalone
libbson. Call from a single thread before any other libbson operation. The
table is copied. Pass ``NULL`` to restore native operations before use.

Applications using libmongoc should call ``mongoc_set_thread_backend``
instead; it configures both libraries. See :symbol:`bson_thread_backend_t` for the callback contract.

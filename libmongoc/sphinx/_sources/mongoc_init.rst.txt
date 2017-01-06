:man_page: mongoc_init

mongoc_init()
=============

Synopsis
--------

.. code-block:: c

  void
  mongoc_init (void);

Description
-----------

This function should be called at the beginning of every program using the MongoDB C driver. It is responsible for initializing global state such as process counters, SSL, and threading primitives.

When your process has completed, you should also call :symbol:`mongoc_cleanup <mongoc_cleanup>`.


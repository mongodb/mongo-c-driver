:man_page: mongoc_init_cleanup

Initialization and cleanup
==========================

Synopsis
--------

The MongoDB C driver must be initialized using :symbol:`mongoc_init <mongoc_init>` before use, and cleaned up with :symbol:`mongoc_cleanup <mongoc_cleanup>` before exiting. Failing to call these functions is a programming error.

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_cleanup
    mongoc_init


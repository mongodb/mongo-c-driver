:man_page: mongoc_init_cleanup

Initialization and cleanup
==========================

Synopsis
--------

.. include:: includes/init_cleanup.txt

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_init
    mongoc_cleanup

.. versionchanged:: 2.0.0 Versions prior to 2.0.0 supported a non-portable automatic initialization and cleanup with the CMake option ``ENABLE_AUTOMATIC_INIT_AND_CLEANUP``. This was removed in 2.0.0. Ensure your application call :symbol:`mongoc_init` and :symbol:`mongoc_cleanup`.

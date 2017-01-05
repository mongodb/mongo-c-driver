:man_page: mongoc_cleanup

mongoc_cleanup()
================

Synopsis
--------

.. code-block:: none

  void
  mongoc_cleanup (void);

Description
-----------

This function is responsible for cleaning up after use of the MongoDB C driver. It will release any lingering allocated memory which can be useful when running under valgrind.


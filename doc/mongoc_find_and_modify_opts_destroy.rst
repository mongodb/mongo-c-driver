:man_page: mongoc_find_and_modify_opts_destroy

mongoc_find_and_modify_opts_destroy()
=====================================

Synopsis
--------

.. code-block:: none

  void
  mongoc_find_and_modify_opts_destroy (mongoc_find_and_modify_opts_t *find_and_modify_opts);

.. tip::

  New in mongoc 1.3.0

Parameters
----------

* ``find_and_modify_opts``: A :symbol:`mongoc_find_and_modify_opts_t <mongoc_find_and_modify_opts_t>`.

Description
-----------

Frees all resources associated with the find and modify builder structure.


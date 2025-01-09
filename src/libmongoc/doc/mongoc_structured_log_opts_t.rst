:man_page: mongoc_structured_log_opts_t

mongoc_structured_log_opts_t
============================

Synopsis
--------

.. code-block:: c

  typedef struct mongoc_structured_log_opts_t mongoc_structured_log_opts_t;

``mongoc_structured_log_opts_t`` is an opaque type that contains options for the structured logging subsystem: per-component log levels, a maximum logged document length, and a handler function.

Create a ``mongoc_structured_log_opts_t`` with :symbol:`mongoc_structured_log_opts_new`, set options and a callback on it, then pass it to :symbol:`mongoc_client_set_structured_log_opts` or :symbol:`mongoc_client_pool_set_structured_log_opts`.
Must be destroyed by calling :symbol:`mongoc_structured_log_opts_destroy`.

Functions
---------

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_opts_new
  mongoc_structured_log_opts_destroy
  mongoc_structured_log_opts_set_handler
  mongoc_structured_log_opts_set_max_level_for_component
  mongoc_structured_log_opts_set_max_level_for_all_components
  mongoc_structured_log_opts_set_max_levels_from_env
  mongoc_structured_log_opts_get_max_level_for_component

.. seealso::

  | :doc:`structured_log`

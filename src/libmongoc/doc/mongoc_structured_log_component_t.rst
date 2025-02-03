:man_page: mongoc_structured_log_component_t

mongoc_structured_log_component_t
=================================

Synopsis
--------

.. code-block:: c

  typedef enum {
     MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND = 0,
     MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY = 1,
     MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION = 2,
     MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION = 3,
  } mongoc_structured_log_component_t;

``mongoc_structured_log_component_t`` enumerates the structured logging components.
Applications should never rely on having an exhaustive list of all log components.
Instead, use :symbol:`mongoc_structured_log_opts_set_max_level_for_all_components` to set a default level if needed.

Functions
---------

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_get_component_name
  mongoc_structured_log_get_named_component

.. seealso::

  | :doc:`structured_log`

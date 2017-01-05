:man_page: mongoc_index

MongoDB C Driver
================

A Cross Platform MongoDB Client Library for C

Introduction
------------

The MongoDB C Driver, also known as "libmongoc", is a library for using MongoDB from C applications, and for writing MongoDB drivers in higher-level languages.

It depends on :doc:`libbson <bson:index>` to generate and parse BSON documents, the native data format of MongoDB.

Installation
------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

Tutorial
--------

Basic Operations
----------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc-common-task-examples

Advanced Connections
--------------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    advanced-connections

Authentication
--------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    authentication

Cursors
-------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

Bulk Operations
---------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bulk

Aggregation Framework
---------------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    aggregate

Client Side Document Matching
-----------------------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    matcher

Troubleshooting
---------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

.. _index_api_reference:

.. _index_api_reference:

API Reference
-------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    init-cleanup
    logging
    mongoc_bulk_operation_t
    mongoc_client_pool_t
    mongoc_client_t
    mongoc_collection_t
    mongoc_cursor_t
    mongoc_database_t
    mongoc_delete_flags_t
    mongoc_find_and_modify_opts_t
    mongoc_gridfs_file_list_t
    mongoc_gridfs_file_opt_t
    mongoc_gridfs_file_t
    mongoc_gridfs_t
    mongoc_host_list_t
    mongoc_index_opt_geo_t
    mongoc_index_opt_t
    mongoc_index_opt_wt_t
    mongoc_insert_flags_t
    mongoc_iovec_t
    mongoc_matcher_t
    mongoc_query_flags_t
    mongoc_rand
    mongoc_read_concern_t
    mongoc_read_mode_t
    mongoc_read_prefs_t
    mongoc_remove_flags_t
    mongoc_reply_flags_t
    mongoc_server_description_t
    mongoc_socket_t
    mongoc_ssl_opt_t
    mongoc_stream_buffered_t
    mongoc_stream_file_t
    mongoc_stream_gridfs_t
    mongoc_stream_socket_t
    mongoc_stream_t
    mongoc_stream_tls_t
    mongoc_topology_description_t
    mongoc_update_flags_t
    mongoc_uri_t
    mongoc_write_concern_t

Errors
------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    errors

Application Performance Monitoring
----------------------------------

.. only:: html

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    application-performance-monitoring
    mongoc_apm_callbacks_t
    mongoc_apm_command_failed_t
    mongoc_apm_command_started_t
    mongoc_apm_command_succeeded_t
    mongoc_apm_server_changed_t
    mongoc_apm_server_closed_t
    mongoc_apm_server_heartbeat_failed_t
    mongoc_apm_server_heartbeat_started_t
    mongoc_apm_server_heartbeat_succeeded_t
    mongoc_apm_server_opening_t
    mongoc_apm_topology_changed_t
    mongoc_apm_topology_closed_t
    mongoc_apm_topology_opening_t


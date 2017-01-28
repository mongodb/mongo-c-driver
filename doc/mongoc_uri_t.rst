:man_page: mongoc_uri_t

mongoc_uri_t
============

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_uri_t mongoc_uri_t;

Description
-----------

``mongoc_uri_t`` provides an abstraction on top of the MongoDB connection URI format. It provides standardized parsing as well as convenience methods for extracting useful information such as replica hosts or authorization information.

See `Connection String URI Reference <http://docs.mongodb.org/manual/reference/connection-string/>`_ on the MongoDB website for more information.

Format
------

.. code-block:: none

  mongodb://                                   <1>
     [username:password@]                      <2>
     host1                                     <3>
     [:port1]                                  <4>
     [,host2[:port2],...[,hostN[:portN]]]      <5>
     [/[database]                              <6>
     [?options]]                               <7>

#. mongodb is the specifier of the MongoDB protocol.
#. An optional username and password.
#. The only required part of the uri.  This specifies either a hostname, IP address or UNIX domain socket.
#. An optional port number.  Defaults to :27017.
#. Extra optional hosts and ports.  You would specify multiple hosts, for example, for connections to replica sets.
#. The name of the database to authenticate if the connection string includes authentication credentials.  If /database is not specified and the connection string includes credentials, defaults to the 'admin' database.
#. Connection specific options.

Replica Set Example
-------------------

To describe a connection to a replica set named 'test' with the following mongod hosts:

* ``db1.example.com`` on port ``27017``
* ``db2.example.com`` on port ``2500``

You would use the connection string that resembles the following.

.. code-block:: none

  mongodb://db1.example.com,db2.example.com:2500/?replicaSet=test

Connection Options
------------------

================  =========================================================================================================================================================================================================================
ssl               {true|false}, indicating if SSL must be used. (See also :symbol:`mongoc_client_set_ssl_opts` and :symbol:`mongoc_client_pool_set_ssl_opts`.)
connectTimeoutMS  A timeout in milliseconds to attempt a connection before timing out. This setting applies to server discovery and monitoring connections as well as to connections for application operations. The default is 10 seconds.
socketTimeoutMS   The time in milliseconds to attempt to send or receive on a socket before the attempt times out. The default is 5 minutes.
================  =========================================================================================================================================================================================================================

Setting any of the \*TimeoutMS options above to ``0`` will be interpreted as "use the default value".

Server Discovery, Monitoring, and Selection Options
---------------------------------------------------

Clients in a :symbol:`mongoc_client_pool_t` share a topology scanner that runs on a background thread. The thread wakes every ``heartbeatFrequencyMS`` (default 10 seconds) to scan all MongoDB servers in parallel. Whenever an application operation requires a server that is not known--for example, if there is no known primary and your application attempts an insert--the thread rescans all servers every half-second. In this situation the pooled client waits up to ``serverSelectionTimeoutMS`` (default 30 seconds) for the thread to find a server suitable for the operation, then returns an error with domain ``MONGOC_ERROR_SERVER_SELECTION``.

Technically, the total time an operation may wait while a pooled client scans the topology is controlled both by ``serverSelectionTimeoutMS`` and ``connectTimeoutMS``. The longest wait occurs if the last scan begins just at the end of the selection timeout, and a slow or down server requires the full connection timeout before the client gives up.

A non-pooled client is single-threaded. Every ``heartbeatFrequencyMS``, it blocks the next application operation while it does a parallel scan. This scan takes as long as needed to check the slowest server: roughly ``connectTimeoutMS``. Therefore the default ``heartbeatFrequencyMS`` for single-threaded clients is greater than for pooled clients: 60 seconds.

By default, single-threaded (non-pooled) clients scan only once when an operation requires a server that is not known. If you attempt an insert and there is no known primary, the client checks all servers once trying to find it, then succeeds or returns an error with domain ``MONGOC_ERROR_SERVER_SELECTION``. But if you set ``serverSelectionTryOnce`` to "false", the single-threaded client loops, checking all servers every half-second, until ``serverSelectionTimeoutMS``.

The total time an operation may wait for a single-threaded client to scan the topology is determined by ``connectTimeoutMS`` in the try-once case, or ``serverSelectionTimeoutMS`` and ``connectTimeoutMS`` if ``serverSelectionTryOnce`` is set "false".

+--------------------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| heartbeatFrequencyMS     | The interval between server monitoring checks. Defaults to 10 seconds in pooled (multi-threaded) mode, 60 seconds in non-pooled mode (single-threaded).                                                                                                                                                                                                                                                  |
+--------------------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| serverSelectionTimeoutMS | A timeout in milliseconds to block for server selection before throwing an exception. The default is 30 seconds.                                                                                                                                                                                                                                                                                         |
+--------------------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| serverSelectionTryOnce   | If "true", the driver scans the topology exactly once after server selection fails, then either selects a server or returns an error. If it is false, then the driver repeatedly searches for a suitable server for up to ``serverSelectionTimeoutMS`` milliseconds (pausing a half second between attempts). The default for ``serverSelectionTryOnce`` is "false" for pooled clients, otherwise "true".|
|                          |                                                                                                                                                                                                                                                                                                                                                                                                          |
|                          | Pooled clients ignore serverSelectionTryOnce; they signal the thread to rescan the topology every half-second until serverSelectionTimeoutMS expires.                                                                                                                                                                                                                                                    |
+--------------------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| socketCheckIntervalMS    | Only applies to single threaded clients. If a socket has not been used within this time, its connection is checked with a quick "isMaster" call before it is used again. Defaults to 5 seconds.                                                                                                                                                                                                          |
+--------------------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+

Setting any of the \*TimeoutMS options above to ``0`` will be interpreted as "use the default value".

Connection Pool Options
-----------------------

These options govern the behavior of a :symbol:`mongoc_client_pool_t`. They are ignored by a non-pooled :symbol:`mongoc_client_t`.

==================  ===============================================================================================================================================================================================================================================================================================
maxPoolSize         The maximum number of clients created by a :symbol:`mongoc_client_pool_t` total (both in the pool and checked out). The default value is 100. Once it is reached, :symbol:`mongoc_client_pool_pop` blocks until another thread pushes a client.
minPoolSize         The number of clients to keep in the pool; once it is reached, :symbol:`mongoc_client_pool_push` destroys clients instead of pushing them. The default value, 0, means "no minimum": a client pushed into the pool is always stored, not destroyed.                  
maxIdleTimeMS       Not implemented.                                                                                                                                                                                                                                                                               
waitQueueMultiple   Not implemented.                                                                                                                                                                                                                                                                               
waitQueueTimeoutMS  Not implemented.                                                                                                                                                                                                                                                                               
==================  ===============================================================================================================================================================================================================================================================================================

.. _mongoc_uri_t_write_concern_options:

Write Concern Options
---------------------

+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| w          | 0          | The driver will not acknowledge write operations but will pass or handle any network and socket errors that it receives to the client. If you disable write concern but enable the getLastError command’s w option, w overrides the w option.                                                                                                                       |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|            | 1          | Provides basic acknowledgment of write operations. By specifying 1, you require that a standalone mongod instance, or the primary for replica sets, acknowledge all write operations. For drivers released after the default write concern change, this is the default write concern setting.                                                                       |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|            | majority   | For replica sets, if you specify the special majority value to w option, write operations will only return successfully after a majority of the configured replica set members have acknowledged the write operation.                                                                                                                                               |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|            | n          | For replica sets, if you specify a number n greater than 1, operations with this write concern return only after n members of the set have acknowledged the write. If you set n to a number that is greater than the number of available set members or members that hold data, MongoDB will wait, potentially indefinitely, for these members to become available. |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|            | tags       | For replica sets, you can specify a tag set to require that all members of the set that have these tags configured return confirmation of the write operation.                                                                                                                                                                                                      |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| wtimeoutMS |            | The time in milliseconds to wait for replication to succeed, as specified in the w option, before timing out. When wtimeoutMS is 0, write operations will never time out.                                                                                                                                                                                           |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| journal    |            | Controls whether write operations will wait until the mongod acknowledges the write operations and commits the data to the on disk journal.                                                                                                                                                                                                                         |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|            | true       | Enables journal commit acknowledgment write concern. Equivalent to specifying the getLastError command with the j option enabled.                                                                                                                                                                                                                                   |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|            | false      | Does not require that mongod commit write operations to the journal before acknowledging the write operation. This is the default option for the journal parameter.                                                                                                                                                                                                 |
+------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+

.. _mongoc_uri_t_read_concern_options:

Read Concern Options
--------------------

================  =============================================================================================================================================================================================================================
readConcernLevel  The level of isolation for read operations. If the level is left unspecified, the server default will be used. See `readConcern in the MongoDB Manual <https://docs.mongodb.org/master/reference/readConcern/>`_ for details.
================  =============================================================================================================================================================================================================================

.. _mongoc_uri_t_read_prefs_options:

Read Preference Options
-----------------------

When connected to a replica set, the driver chooses which member to query using the read preference:

#. Choose members whose type matches "readPreference".
#. From these, if there are any tags sets configured, choose members matching the first tag set. If there are none, fall back to the next tag set and so on, until some members are chosen or the tag sets are exhausted.
#. From the chosen servers, distribute queries randomly among the server with the fastest round-trip times. These include the server with the fastest time and any whose round-trip time is no more than "localThresholdMS" slower.

==================  =======================================================================================================================================================================
readPreference      Specifies the replica set read preference for this connection. This setting overrides any slaveOk value. The read preference values are the following:

                    * primary (default)
                    * primaryPreferred
                    * secondary
                    * secondaryPreferred
                    * nearest





readPreferenceTags  Specifies a tag set as a comma-separated list of colon-separated key-value pairs.

                    Cannot be combined with preference "primary".

localThresholdMS    How far to distribute queries, beyond the server with the fastest round-trip time. By default, only servers within 15ms of the fastest round-trip time receive queries.
==================  =======================================================================================================================================================================

.. note::

  "localThresholdMS" is ignored when talking to replica sets through a mongos. The equivalent is `mongos's localThreshold command line option <https://docs.mongodb.org/manual/reference/program/mongos/#cmdoption--localThreshold>`_.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_uri_copy
    mongoc_uri_destroy
    mongoc_uri_get_auth_mechanism
    mongoc_uri_get_auth_source
    mongoc_uri_get_database
    mongoc_uri_get_hosts
    mongoc_uri_get_mechanism_properties
    mongoc_uri_get_option_as_bool
    mongoc_uri_get_option_as_int32
    mongoc_uri_get_option_as_utf8
    mongoc_uri_get_options
    mongoc_uri_get_password
    mongoc_uri_get_read_concern
    mongoc_uri_get_read_prefs
    mongoc_uri_get_read_prefs_t
    mongoc_uri_get_replica_set
    mongoc_uri_get_ssl
    mongoc_uri_get_string
    mongoc_uri_get_username
    mongoc_uri_get_write_concern
    mongoc_uri_new
    mongoc_uri_new_for_host_port
    mongoc_uri_option_is_bool
    mongoc_uri_option_is_int32
    mongoc_uri_option_is_utf8
    mongoc_uri_set_auth_source
    mongoc_uri_set_database
    mongoc_uri_set_mechanism_properties
    mongoc_uri_set_option_as_bool
    mongoc_uri_set_option_as_int32
    mongoc_uri_set_option_as_utf8
    mongoc_uri_set_password
    mongoc_uri_set_read_concern
    mongoc_uri_set_read_prefs_t
    mongoc_uri_set_username
    mongoc_uri_set_write_concern
    mongoc_uri_unescape


:man_page: mongoc_advanced_connections

Advanced Connections
====================

The following guide contains information specific to certain types of MongoDB configurations.

For an example of connecting to a simple standalone server, see the :ref:`Tutorial <tutorial_connecting>`. To establish a connection with authentication options enabled, see the :doc:`Authentication <authentication>` page. To see an example of a connection with data compression, see the :doc:`Data Compression <data-compression>` page.

Connecting to a Replica Set
---------------------------

Connecting to a `replica set <https://www.mongodb.com/docs/manual/replication/>`_ is much like connecting to a standalone MongoDB server. Simply specify the replica set name using the ``?replicaSet=myreplset`` URI option.

.. code-block:: c

  #include <bson/bson.h>
  #include <mongoc/mongoc.h>

  int
  main (int argc, char *argv[])
  {
     mongoc_client_t *client;

     mongoc_init ();

     /* Create our MongoDB Client */
     client = mongoc_client_new (
        "mongodb://host01:27017,host02:27017,host03:27017/?replicaSet=myreplset");

     /* Do some work */
     /* TODO */

     /* Clean up */
     mongoc_client_destroy (client);
     mongoc_cleanup ();

     return 0;
  }

.. tip::

  Multiple hostnames can be specified in the MongoDB connection string URI, with a comma separating hosts in the seed list.

  It is recommended to use a seed list of members of the replica set to allow the driver to connect to any node.

Connecting to a Sharded Cluster
-------------------------------

To connect to a `sharded cluster <https://www.mongodb.com/docs/manual/sharding/>`_, specify the ``mongos`` nodes the client should connect to. The C Driver will automatically detect that it has connected to a ``mongos`` sharding server.

If more than one hostname is specified, a seed list will be created to attempt failover between the ``mongos`` instances.

.. warning::

  Specifying the ``replicaSet`` parameter when connecting to a ``mongos`` sharding server is invalid.

.. code-block:: c

  #include <bson/bson.h>
  #include <mongoc/mongoc.h>

  int
  main (int argc, char *argv[])
  {
     mongoc_client_t *client;

     mongoc_init ();

     /* Create our MongoDB Client */
     client = mongoc_client_new ("mongodb://myshard01:27017/");

     /* Do something with client ... */

     /* Free the client */
     mongoc_client_destroy (client);

     mongoc_cleanup ();

     return 0;
  }

Connecting to an IPv6 Address
-----------------------------

The MongoDB C Driver will automatically resolve IPv6 addresses from host names. However, to specify an IPv6 address directly, wrap the address in ``[]``.

.. code-block:: none

  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://[::1]:27017");

Connecting with IPv4 and IPv6
-----------------------------

.. include:: includes/ipv4-and-ipv6.txt

Connecting to a UNIX Domain Socket
----------------------------------

On UNIX-like systems, the C Driver can connect directly to a MongoDB server using a UNIX domain socket. Pass the URL-encoded path to the socket, which *must* be suffixed with ``.sock``. For example, to connect to a domain socket at ``/tmp/mongodb-27017.sock``:

.. code-block:: none

  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://%2Ftmp%2Fmongodb-27017.sock");

Include username and password like so:

.. code-block:: none

  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://user:pass@%2Ftmp%2Fmongodb-27017.sock");


Connecting to a server over TLS
-------------------------------

These are instructions for configuring TLS/SSL connections.

To run a server locally (on port 27017, for example):

.. code-block:: none

  $ mongod --port 27017 --tlsMode requireTLS --tlsCertificateKeyFile server.pem --tlsCAFile ca.pem

Add ``/?tls=true`` to the end of a client URI.

.. code-block:: none

  mongoc_client_t *client = NULL;
  client = mongoc_client_new ("mongodb://localhost:27017/?tls=true");

MongoDB requires client certificates by default, unless the ``--tlsAllowConnectionsWithoutCertificates`` is provided. The C Driver can be configured to present a client certificate using the URI option ``tlsCertificateKeyFile``, which may be referenced through the constant ``MONGOC_URI_TLSCERTIFICATEKEYFILE``.

.. code-block:: none

  mongoc_client_t *client = NULL;
  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?tls=true");
  mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, "client.pem");

  client = mongoc_client_new_from_uri (uri);

The client certificate provided by ``tlsCertificateKeyFile`` must be issued by one of the server trusted Certificate Authorities listed in ``--tlsCAFile``, or issued by a CA in the native certificate store on the server when omitted.

See :doc:`configuring_tls` for more information on the various TLS related options.

Compressing data to and from MongoDB
------------------------------------

This content has been relocated to the :doc:`Data Compression <data-compression>` page.

Additional Connection Options
-----------------------------

The full list of connection options can be found in the :symbol:`mongoc_uri_t` docs.

Certain socket/connection related options are not configurable:

============== ===================================================== ======================
Option         Description                                           Value
============== ===================================================== ======================
SO_KEEPALIVE   TCP Keep Alive                                        Enabled
-------------- ----------------------------------------------------- ----------------------
TCP_KEEPIDLE   How long a connection needs to remain idle before TCP 120 seconds
               starts sending keepalive probes
-------------- ----------------------------------------------------- ----------------------
TCP_KEEPINTVL  The time in seconds between TCP probes                10 seconds
-------------- ----------------------------------------------------- ----------------------
TCP_KEEPCNT    How many probes to send, without acknowledgement,     9 probes
               before dropping the connection
-------------- ----------------------------------------------------- ----------------------
TCP_NODELAY    Send packets as soon as possible or buffer small      Enabled (no buffering)
               packets (Nagle algorithm)
============== ===================================================== ======================


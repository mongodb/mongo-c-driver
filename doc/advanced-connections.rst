:man_page: mongoc_advanced_connections

Advanced Connections
====================

The following guide contains information specific to certain types of MongoDB configurations.

For an example of connecting to a simple standalone server, see the :ref:`Tutorial <tutorial_connecting>`. To establish a connection with authentication options enabled, see the :doc:`Authentication <authentication>` page.

Connecting to a Replica Set
---------------------------

Connecting to a `replica set <http://docs.mongodb.org/manual/replication/>`_ is much like connecting to a standalone MongoDB server. Simply specify the replica set name using the ``?replicaSet=myreplset`` URI option.

.. code-block:: c

  #include <bson.h>
  #include <mongoc.h>

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

To connect to a `sharded cluster <http://docs.mongodb.org/manual/sharding/>`_, specify the ``mongos`` nodes the client should connect to. The C Driver will automatically detect that it has connected to a ``mongos`` sharding server.

If more than one hostname is specified, a seed list will be created to attempt failover between the ``mongos`` instances.

.. warning::

  Specifying the ``replicaSet`` parameter when connecting to a ``mongos`` sharding server is invalid.

.. code-block:: c

  #include <bson.h>
  #include <mongoc.h>

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

Connecting to a UNIX Domain Socket
----------------------------------

On UNIX-like systems, the C Driver can connect directly to a MongoDB server using a UNIX domain socket. Pass the URL-encoded path to the socket, which *must* be suffixed with ``.sock``. For example, to connect to a domain socket at ``/tmp/mongodb-27017.sock``:

.. code-block:: none

  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://%2Ftmp%2Fmongodb-27017.sock");

Include username and password like so:

.. code-block:: none

  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://user:pass@%2Ftmp%2Fmongodb-27017.sock");

Connecting to a server over SSL
-------------------------------

These are instructions for configuring TLS/SSL connections.

To run a server locally (on port 27017, for example):

.. code-block:: none

  $ mongod --port 27017 --sslMode requireSSL --sslPEMKeyFile server.pem --sslCAFile ca.pem 

Add ``/?ssl=true`` to the end of a client URI.

.. code-block:: none

  mongoc_client_t *client = NULL;
  client = mongoc_client_new ("mongodb://localhost:27017/?ssl=true");

MongoDB requires client certificates by default, unless the ``--sslAllowConnectionsWithoutCertificates`` is provided. The C Driver can be configured to present a client certificate using a ``mongoc_ssl_opt_t``:

.. code-block:: none

  const mongoc_ssl_opt_t *ssl_default = mongoc_ssl_opt_get_default ();
  mongoc_ssl_opt_t ssl_opts = { 0 };

  /* optionally copy in a custom trust directory or file; otherwise the default is used. */
  memcpy (&ssl_opts, ssl_default, sizeof ssl_opts);
  ssl_opts.pem_file = "client.pem" 

  mongoc_client_set_ssl_opts (client, &ssl_opts);

The client certificate provided by ``pem_file`` must be issued by one of the server trusted Certificate Authorities listed in ``--sslCAFile``, or issued by a CA in the native certificate store on the server when omitted.

To verify the server certificate against a specific CA, provide a PEM armored file with a CA certificate, or contatinated list of CA certificates using the ``ca_file`` option, or ``c_rehash`` directory structure of CAs, pointed to using the ``ca_dir`` option. When no ``ca_file`` or ``ca_dir`` is provided, the driver will use CAs provided by the native platform certificate store.

See :doc:`mongoc_ssl_opt_t` for more information on the various SSL related options.

Additional Connection Options
-----------------------------

A variety of connection options for the MongoDB URI can be found `here <http://docs.mongodb.org/manual/reference/connection-string/>`_.


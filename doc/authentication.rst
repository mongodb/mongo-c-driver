:man_page: mongoc_authentication

Authentication
==============

This guide covers the use of authentication options with the MongoDB C Driver. Ensure that the MongoDB server is also properly configured for authentication before making a connection. For more information, see the `MongoDB security documentation <https://docs.mongodb.org/manual/administration/security/>`_.

Basic Authentication
--------------------

The MongoDB C driver supports challenge response authentication (sometimes known as ``MONGODB-CR``) through the use of MongoDB connection URIs.

Simply provide the username and password as one would with an ``HTTP URL``, as well as the database to authenticate against via ``authSource``.

.. code-block:: none

  mongoc_client_t *client = mongoc_client_new ("mongodb://user:password@localhost/?authSource=mydb");

.. _authentication_kerberos:

GSSAPI (Kerberos) Authentication
--------------------------------

.. note::

  Kerberos support is only provided in environments supported by the ``cyrus-sasl`` Kerberos implementation. This currently limits support to UNIX-like environments.

``GSSAPI`` (Kerberos) authentication is available in the Enterprise Edition of MongoDB, version 2.4 and newer. To authenticate using ``GSSAPI``, the MongoDB C driver must be installed with SASL support. Run the ``kinit`` command before using the following authentication methods:

.. code-block:: none

  $ kinit mongodbuser@EXAMPLE.COMmongodbuser@EXAMPLE.COM's Password:
  $ klistCredentials cache: FILE:/tmp/krb5cc_1000
          Principal: mongodbuser@EXAMPLE.COM

    Issued                Expires               Principal
  Feb  9 13:48:51 2013  Feb  9 23:48:51 2013  krbtgt/EXAMPLE.COM@EXAMPLE.COM

Now authenticate using the MongoDB URI. ``GSSAPI`` authenticates against the ``$external`` virtual database, so a database does not need to be specified in the URI. Note that the Kerberos principal *must* be URL-encoded:

.. code-block:: none

  mongoc_client_t *client;

  client = mongoc_client_new ("mongodb://mongodbuser%40EXAMPLE.COM@example.com/?authMechanism=GSSAPI");

The driver supports these GSSAPI properties:

* ``CANONICALIZE_HOST_NAME``: This might be required when the hosts report different hostnames than what is used in the kerberos database. The default is "false".
* ``SERVICE_NAME``: Use a different service name than the default, "mongodb".

Set properties in the URL:

.. code-block:: none

  mongoc_client_t *client;

  client = mongoc_client_new ("mongodb://mongodbuser%40EXAMPLE.COM@example.com/?authMechanism=GSSAPI&"
                              "authMechanismProperties=SERVICE_NAME:other,CANONICALIZE_HOST_NAME:true");

If you encounter errors such as ``Invalid net address``, check if the application is behind a NAT (Network Address Translation) firewall. If so, create a ticket that uses ``forwardable`` and ``addressless`` Kerberos tickets. This can be done by passing ``-f -A`` to ``kinit``.

.. code-block:: none

  $ kinit -f -A mongodbuser@EXAMPLE.COM

SASL Plain Authentication
-------------------------

.. note::

  The MongoDB C Driver must be compiled with SASL support in order to use ``SASL PLAIN`` authentication.

MongoDB Enterprise Edition versions 2.5.0 and newer support the ``SASL PLAIN`` authentication mechanism, initially intended for delegating authentication to an LDAP server. Using the ``SASL PLAIN`` mechanism is very similar to the challenge response mechanism with usernames and passwords. These examples use the ``$external`` virtual database for ``LDAP`` support:

.. note::

  ``SASL PLAIN`` is a clear-text authentication mechanism. It is strongly recommended to connect to MongoDB using SSL with certificate validation when using the ``PLAIN`` mechanism.

.. code-block:: none

  mongoc_client_t *client;

  client = mongoc_client_new ("mongodb://user:password@example.com/?authMechanism=PLAIN&authSource=$external");

X.509 Certificate Authentication
--------------------------------

.. note::

  The MongoDB C Driver must be compiled with SSL support for X.509 authentication support. Once this is done, start a server with the following options: 

  .. code-block:: none

    $ mongod --clusterAuthMode x509 --sslMode requireSSL --sslPEMKeyFile server.pem --sslCAFile ca.pem

The ``MONGODB-X509`` mechanism authenticates a username derived from the distinguished subject name of the X.509 certificate presented by the driver during SSL negotiation. This authentication method requires the use of SSL connections with certificate validation and is available in MongoDB 2.5.1 and newer:

.. code-block:: none

  mongoc_client_t *client;
  mongoc_ssl_opt_t ssl_opts = { 0 };

  ssl_opts.pem_file = "mycert.pem";
  ssl_opts.pem_pwd = "mycertpassword";
  ssl_opts.ca_file = "myca.pem";
  ssl_opts.ca_dir = "trust_dir";
  ssl_opts.weak_cert_validation = false;

  client = mongoc_client_new ("mongodb://x509_derived_username@localhost/?authMechanism=MONGODB-X509");
  mongoc_client_set_ssl_opts (client, &ssl_opts);

``MONGODB-X509`` authenticates against the ``$external`` database, so specifying a database is not required. For more information on the x509_derived_username, see the MongoDB server `x.509 tutorial <https://docs.mongodb.com/manual/tutorial/configure-x509-client-authentication/#add-x-509-certificate-subject-as-a-user>`_.


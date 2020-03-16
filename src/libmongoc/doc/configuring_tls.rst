:man_page: configuring_tls

Configuring TLS
===============

Building libmongoc with TLS support
-----------------------------------

By default, libmongoc will attempt to find a supported TLS library and enable TLS support. This is controlled by the cmake flag ``ENABLE_SSL``, which is set to ``AUTO`` by default. Valid values are:

- ``AUTO`` the default behavior. Link to the system's native TLS library, or attempt to find OpenSSL.
- ``DARWIN`` link to Secure Transport, the native TLS library on macOS.
- ``WINDOWS`` link to Secure Channel, the native TLS on Windows.
- ``OPENSSL`` link to OpenSSL (libssl). An optional install path may be specified with ``OPENSSL_ROOT``.
- ``LIBRESSL`` link to LibreSSL's libtls. (LibreSSL's compatible libssl may be linked to by setting ``OPENSSL``).
- ``OFF`` disable TLS support.

Configuration with URI options
------------------------------

Enable TLS by including ``tls=true`` the URI.

.. code:: c
   
  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/");
  mongoc_uri_set_option_as_bool (uri, MONGOC_URI_TLS, true);

  mongoc_client_t *client = mongoc_client_new_from_uri (uri);


The following URI options may be used to further configure TLS:

.. include:: includes/tls-options.txt

Configuration with mongoc_ssl_opt_t
-----------------------------------

Alternatively, the :symbol:`mongoc_ssl_opt_t` struct may be used to configure TLS with :symbol:`mongoc_client_set_ssl_opts()` or :symbol:`mongoc_client_pool_set_ssl_opts()`. Most of the configurable options can be using the Connection URI.

===============================  ===============================
**mongoc_ssl_opt_t key**         **URI key**
===============================  ===============================
pem_file                         tlsClientCertificateKeyFile
pem_pwd                          tlsClientCertificateKeyPassword
ca_file                          tlsCAFile
weak_cert_validation             tlsAllowInvalidCertificates
allow_invalid_hostname           tlsAllowInvalidHostnames
===============================  ===============================

The only exclusions are ``crl_file`` and ``ca_dir``. Those may only be set with :symbol:`mongoc_ssl_opt_t`.

Client Authentication
---------------------

When MongoDB is started with TLS enabled, it will by default require the client to provide a client certificate issued by a certificate authority specified by ``--tlsCAFile``, or an authority trusted by the native certificate store in use on the server.

To provide the client certificate, set the ``tlscertificatekeyfile`` in the URI to a PEM armored certificate file.

.. code-block:: c

  mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/");
  mongoc_uri_set_option_as_bool (uri, MONGOC_URI_TLS, true);
  mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, "/path/to/client-certificate.pem");

  mongoc_client_t *client = mongoc_client_new_from_uri (uri);

Server Certificate Verification
-------------------------------

The MongoDB C Driver will automatically verify the validity of the server certificate, such as issued by configured Certificate Authority, hostname validation, and expiration.

To overwrite this behaviour, it is possible to disable hostname validation, and/or allow otherwise invalid certificates. This behaviour is controlled using the ``tlsallowinvalidhostnames`` and ``tlsallowinvalidcertificates`` options. By default, both are set to ``false``. It is not recommended to change these defaults as it exposes the client to *Man In The Middle* attacks (when ``tlsallowinvalidhostnames`` is set) and otherwise invalid certificates when ``tlsallowinvalidcertificates`` is set to ``true``.

OpenSSL
-------

The MongoDB C Driver uses OpenSSL, if available, on Linux and Unix platforms (besides macOS). Industry best practices and some regulations require the use of TLS 1.1 or newer, which requires at least OpenSSL 1.0.1. Check your OpenSSL version like so::

  $ openssl version

Ensure your system's OpenSSL is a recent version (at least 1.0.1), or install a recent version in a non-system path and build against it with::

  cmake -DOPENSSL_ROOT_DIR=/absolute/path/to/openssl

When compiled against OpenSSL, the driver will attempt to load the system default certificate store, as configured by the distribution. That can be overridden by setting the ``tlscafile`` URI option or with the fields ``ca_file`` and ``ca_dir`` in the :symbol:`mongoc_ssl_opt_t`.

LibreSSL / libtls
-----------------

The MongoDB C Driver supports LibreSSL through the use of OpenSSL compatibility checks when configured to compile against ``openssl``. It also supports the new ``libtls`` library when configured to build against ``libressl``.

Native TLS Support on Windows (Secure Channel)
----------------------------------------------

The MongoDB C Driver supports the Windows native TLS library (Secure Channel, or SChannel), and its native crypto library (Cryptography API: Next Generation, or CNG).

When compiled against the Windows native libraries, the ``ca_dir`` option of a :symbol:`mongoc_ssl_opt_t` is not supported, and will issue an error if used.

Encrypted PEM files (e.g., setting ``tlscertificatekeypassword``) are also not supported, and will result in error when attempting to load them.

When ``tlscafile`` is set, the driver will only allow server certificates issued by the authority (or authorities) provided. When no ``tlscafile`` is set, the driver will look up the Certificate Authority using the ``System Local Machine Root`` certificate store to confirm the provided certificate or the ``Current user certificate store`` if the ``System Local Machine Root`` certificate store is unavailable.

When ``crl_file`` is set with :symbol:`mongoc_ssl_opt_t`, the driver will import the revocation list to the ``System Local Machine Root`` certificate store.

.. _Secure Transport:

Native TLS Support on macOS / Darwin (Secure Transport)
-------------------------------------------------------

The MongoDB C Driver supports the Darwin (OS X, macOS, iOS, etc.) native TLS library (Secure Transport), and its native crypto library (Common Crypto, or CC).

When compiled against Secure Transport, the ``ca_dir`` option of a :symbol:`mongoc_ssl_opt_t` is not supported, and will issue an error if used.

When ``tlscafile`` is set, the driver will only allow server certificates issued by the authority (or authorities) provided. When no ``tlscafile`` is set, the driver will use the Certificate Authorities in the currently unlocked keychains.
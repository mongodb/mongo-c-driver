:man-page: using_client_side_encryption

Using Client-Side Field Level Encryption
========================================

New in MongoDB 4.2, Client-Side Field Level Encryption (also referred to as Client-Side Encryption) allows administrators and developers to encrypt specific data fields in addition to other MongoDB encryption features.

With Client-Side Encryption, developers can encrypt fields client side without any server-side configuration or directives. Client-Side Encryption supports workloads where applications must guarantee that unauthorized parties, including server administrators, cannot read the encrypted data.

Automatic encryption, where sensitive fields in commands are encrypted automatically, requires an Enterprise-only process to do query analysis.

Installation
------------

libmongocrypt
`````````````

There is a separate library, `libmongocrypt <https://github.com/mongodb/libmongocrypt>`_ that must be installed prior to configuring libmongoc to enable Client-Side Encryption.

libmongocrypt depends on libbson. To build libmongoc with Client-Side Encryption support you must:

1. Install libbson
2. Build and install libmongocrypt
3. Build libmongoc

To install libbson, follow the instructions to install with a package manager: :ref:`Install libbson with a Package Manager <installing_libbson_with_pkg_manager>` or build from source with cmake (disable building libmongoc with ``-DENABLE_MONGOC=OFF``):

.. parsed-literal::

  $ cd mongo-c-driver
  $ mkdir cmake-build && cd cmake-build
  $ cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DENABLE_MONGOC=OFF ..
  $ cmake --build . --target install

To build and install libmongocrypt, clone `the repository <https://github.com/mongodb/libmongocrypt>`_ and configure as follows:

.. parsed-literal::

  $ cd libmongocrypt
  $ mkdir cmake-build && cd cmake-build
  $ cmake -DENABLE_SHARED_BSON=ON ..
  $ cmake --build . --target install

Then, you should be able to build libmongoc with Client-Side Encryption.

.. parsed-literal::

  $ cd mongo-c-driver
  $ mkdir cmake-build && cd cmake-build
  $ cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DENABLE_MONGOC=ON -DENABLE_CLIENT_SIDE_ENCRYPTION=ON ..
  $ cmake --build . --target install

mongocryptd
```````````

To enable automatic encryption, install the Enterprise-only binary mongocryptd, which runs as a separate process and performs query analysis.

libmongoc connects to mongocryptd when using automatic encryption. A specific URI can be configured in the :symbol:`mongoc_auto_encryption_opts_t` class by setting mongocryptdURI with :symbol:`mongoc_auto_encryption_opts_set_extra()`.


Examples
--------

The following is a sample app that assumes the data key and schema have already been created in MongoDB. The example uses a local master key, however using AWS Key Management Service is also an option. The data in the ``encryptedField`` field is automatically encrypted on the insert and decrypted when using find on the client side:

.. literalinclude:: ../examples/client-side-encryption-tour.c
   :caption: client-side-encryption-tour.c
   :language: c

.. note::

   Automatic encryption is an **Enterprise** only feature.

The following example shows how to further configure a :symbol:`mongoc_auto_encryption_opts_t`, to create a new data key, and to set the JSON schema map to use that new key.

.. literalinclude:: ../examples/client-side-encryption-options-tour.c
   :caption: client-side-encryption-options-tour.c
   :language: c

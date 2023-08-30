:man_page: mongoc_data_compression

Data Compression
================

The following guide explains how data compression support works between the MongoDB server and client. It also shows an example of how to connect to a server with data compression.

Compressing data to and from MongoDB
------------------------------------

MongoDB 3.4 added Snappy compression support, while zlib compression was added in 3.6, and zstd compression in 4.2.
To enable compression support the client must be configured with which compressors to use:

.. code-block:: none

  mongoc_client_t *client = NULL;
  client = mongoc_client_new ("mongodb://localhost:27017/?compressors=snappy,zlib,zstd");

The ``compressors`` option specifies the priority order of compressors the
client wants to use. Messages are compressed if the client and server share any
compressors in common.

Note that the compressor used by the server might not be the same compressor as
the client used.  For example, if the client uses the connection string
``compressors=zlib,snappy`` the client will use ``zlib`` compression to send
data (if possible), but the server might still reply using ``snappy``,
depending on how the server was configured.

The driver must be built with zlib and/or snappy and/or zstd support to enable compression
support, any unknown (or not compiled in) compressor value will be ignored.


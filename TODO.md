# TODO

This is just a general TODO. It does not imply a roadmap, just things that
I am thinking about and need a place to write them down.

## Clients

## Client Pooling

 * [M2] Shutdown clients when we no longer need them (on push())
 * [M2] Try to keep more recently used (if healthy) and discard old ones.

## Cluster

See Replica Sets and Sharded Cluster.

## Cursors

 * [M2] Detect commands as cursors if possible.

## GridFS

 * [M2] GridFS support adding new mongoc_gridfs_t structure.
   We can add mongoc_client_get_gridfs() to access the structure
   in a similar fashion to get_collection().

## Bulk API

 * [M2] The server is getting new bulk commands for insert/update/etc.
   If we detect that the server supports these, use them instead.

## Replica Sets

 * [M1] Needs more testing on reconnection strategies.
 * [M1] Occasionally perform reconnect when in unhealthy state and
   time period has elapsed. This means keeping the monotonic time we
   last performed a reconnect.

## Sharded Cluster

 * [M1] Still needs implementation, reconnect strategies.

## TLS

 * [M2] Finish support for mongoc_stream_tls_t.
 * [M2] Apply TLS stream if ssl=true in URI.

## Authentication

 * [M2] X509 Certificate Authentication
 * [M2] Kerberos Authentication

## Documentation

 * [M1] mongoc-stat is really useful, needs documentation.
 * [M1] man pages for basic documentation.
 * [M1] Generate API docs.

## Examples

 * [M2] Build an example application using the library.
   Probably something basic that uses HTTP to push data into MongoDB.   

## Asynchronous Support

 * [M4] mongoc_client_async_t
 * [M4] mongoc_collection_async_t
 * [M4] mongoc_database_async_t
 * [M4] mongoc_client_pool_async_t
 * [M4] Be careful with timeouts
 * [M4] Support for GLib main loop, libev, etc.

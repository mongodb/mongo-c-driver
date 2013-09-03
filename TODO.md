# TODO

This is just a general TODO. It does not imply a roadmap, just things that
I am thinking about and need a place to write them down.

## Clients

 * Use mongoc_stream_buffered_t.
   Still needs more implementation work.
 * Respect connectiontimeoutms.
   This requires using poll() and non-blocking I/O for connect().
 * Test what happens during mongoc_client_destroy() with active work.

## Client Pooling

 * Shutdown clients when we no longer need them (on push())
 * Try to keep more recently used (if healthy) and discard old ones.

## Cluster

 * Use local bson_error_t when appropriate.
 * Drop use of sendv() since we handle write-concern now.

## Cursors

 * Detect commands as cursors if possible.

## Bulk API

 * The server is getting new bulk commands for insert/update/etc.
   If we detect that the server supports these, use them instead.

## Replica Sets

 * Needs more testing on reconnection strategies.

## Sharded Cluster

 * Still needs implementation, reconnect strategies.

## TLS

 * Finish support for mongoc_stream_tls_t.
 * Apply TLS stream if ssl=true in URI.

## Counters

 * We can remove the need for integer keys by generating an enum.

## Authentication

 * X509 Certificate Authentication
 * Kerberos Authentication

## Documentation

 * mongoc-state is really useful, needs documentation.
 * man pages for basic documentation.
 * Generate API docs.

## Examples

 * Build an example application using the library.
   Probably something basic that uses HTTP to push data into MongoDB.

## Build System and Library

 * Support library versioning (soname)
 * Add ABI checks for symbols
 * Clean up Makefiles

## Asynchronous Support

 * mongoc_client_async_t
 * mongoc_collection_async_t
 * mongoc_database_async_t
 * mongoc_client_pool_async_t
 * Be careful with timeouts
 * Support for GLib main loop, libev, etc.

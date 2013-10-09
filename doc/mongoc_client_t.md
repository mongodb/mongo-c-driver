# mongoc_client_t

```c
typedef struct _mongoc_client_t mongoc_client_t;
```

`mongoc_client_t` is an opaque structure providing access to a MongoDB node, replica-set, or sharded-cluster. It maintains management of underlying sockets and routing to individual nodes based on `mongoc_read_prefs_t` or `mongoc_write_concern_t`.

----------

## Constructors

### mongoc_client_new

```c
mongoc_client_t *
mongoc_client_new (const char *uri_string);
```

This function creates a new `mongoc_client_t` using the URI described by `uri_string` to connect to the MongoDB node, replica-set, or sharded-cluster.

#### Parameters

 * `uri_string`: `IN`: A string containing the "mongodb://" style URI.

#### Returns

A newly allocated `mongoc_client_t` if successful, otherwise `NULL` if `uri_string` could not be parsed. This should be freed with `mongodb_client_destroy()` when no longer in use.

**Availability**: Since 0.90.0

----------

### mongoc_client_new_from_uri

```c
mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri);
```

This function behaves exactly the same as `mongoc_client_new()` except that it takes an already parsed `mongoc_uri_t`.

#### Parameters

 * `uri`: `IN`: A `mongoc_uri_t` to copy for establishing connections.

#### Returns

A newly allocated `mongoc_client_t` that should be freed with `mongoc_client_destroy()` when no longer used.

**Availability**: Since 0.90.0

----------

## Destructors

### mongoc_client_destroy

```c
void
mongoc_client_destroy (mongoc_client_t *client);
```

This function will release any resources held by `client`. It is a programming error to use `client` after calling this function.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t` that is no longer in use.

**Availability**: Since 0.90.0

----------

## Functions

### mongoc_client_get_uri

```c
const mongoc_uri_t *
mongoc_client_get_uri (const mongoc_client_t *client);
```

This function retrieves the `mongoc_uri_t` used by `client`. It is a programming error to modify the `mongoc_uri_t`.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.

#### Returns

A `mongoc_uri_t` that should not be modified or freed.

**Availability**: Since 0.90.0

----------

### mongoc_client_set_stream_initiator

```c
void
mongoc_client_set_stream_initiator (mongoc_client_t           *client,
                                    mongoc_stream_initiator_t  initiator,
                                    void                      *user_data);
```

Sets the stream initiator that `client` should use to create new connections. Setting `initiator` to `NULL` will use the default initiator.

If you are integrating with an external stream implementation, you may use this to bridge the two systems together. By default, `mongoc_client_t` uses standard BSD sockets to communicate with a MongoDB node.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.
 * `initiator`: `IN`: A `mongoc_stream_initator_t` to call when creating streams.
 * `user_data`: `IN`: User defined data for `initiator`.

#### See Also

See `mongoc_stream_initator_t` for more information on creating streams.

**Availability**: Since 0.90.0

----------

### mongoc_client_get_database

```c
mongoc_database_t *
mongoc_client_get_database (mongoc_client_t *client,
                            const char      *name);
```

This function retrieves a `mongoc_database_t` structure for a named database on `client`. It may be used to execute commands or other database related tasks.

You **MUST** release the resulting database with `mongoc_database_destroy()` before `client` is released.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.
 * `name`: `IN`: The name of the database, such as "test".

#### Returns

A newly allocated `mongoc_database_t` that should be released with `mongoc_database_destroy()`.

**Availability**: Since 0.90.0

----------

### mongoc_client_get_collection

```c
mongoc_collection_t *
mongoc_client_get_collection (mongoc_client_t *client,
                              const char      *db,
                              const char      *collection);
```

This function allocates a new `mongoc_collection_t` that can be used for collection related tasks against MongoDB. The caller **MUST** free the collection using `mongoc_collection_destroy()` before releasing `client`.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.
 * `db`: `IN`: A database name such as "mydb".
 * `collection`: `IN`: A collection name such as "mycollection".

#### Returns

A newly allocated `mongoc_collection_t` that should be freed with `mongoc_collection_destroy()`.

**Availability**: Since 0.90.0

----------

### mongoc_client_get_write_concern

```c
const mongoc_write_concern_t *
mongoc_client_get_write_concern (const mongoc_client_t *client);
```

This function fetches the default `mongoc_write_concern_t` in use for `client`. When no overriding write-concern is set, this is used.

`mongoc_collection_t` and `mongoc_database_t` will inherit this `mongoc_write_concern_t`.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.

#### Returns

A `mongoc_write_concern_t` that **MUST** not be modified or freed.

**Availability**: Since 0.90.0

----------

### mongoc_client_set_write_concern

```c
void
mongoc_client_set_write_concern (mongoc_client_t              *client,
                                 const mongoc_write_concern_t *write_concern);
```

This function sets the default `mongoc_write_concern_t` for `client`. Collections and databases accessed after setting this will default to using `write_concern`.

`write_concern` will be copied and therefore not modified.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.
 * `write_concern`: `IN`: A `mongoc_write_concern_t`.

**Availability**: Since 0.90.0

----------

### mongoc_client_get_read_prefs

```c
const mongoc_read_prefs_t *
mongoc_client_get_read_prefs (const mongoc_client_t *client);
```

This function gets the default `mongoc_read_prefs_t` used by `client`. `mongoc_collection_t` and `mongoc_database_t` created by `client` will default to these read preferences.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.

#### Returns

A `mongoc_read_prefs_t` that should not be modified or freed.

**Availability**: Since 0.90.0

----------

### mongoc_client_set_read_prefs

```c
void
mongoc_client_set_read_prefs (mongoc_client_t              *client,
                              const mongoc_read_prefs_t    *read_prefs);
```

Sets the default read preferences for `client` and the `mongoc_collection_t` or `mongoc_database_t` it creates.

#### Parameters

 * `client`: `IN`: A `mongoc_client_t`.
 * `read_prefs`: `IN`: A `mongoc_read_prefs_t`.

**Availability**: Since 0.90.0

----------

## Types

### mongoc_stream_initiator_t

```c
typedef mongoc_stream_t *(*mongoc_stream_initiator_t) (const mongoc_uri_t       *uri,
                                                       const mongoc_host_list_t *host,
                                                       void                     *user_data,
                                                       bson_error_t             *error);
```

This function definition describes how new `mongoc_stream_t` are created for `mongoc_client_t` instances. By implementing this prototype and calling `mongoc_client_set_stream_initiator()` you may control the creation of communication transports.

Some language bindings may choose to use this to use their languages stream abstraction rather than BSD sockets as part of libc. For example, if your language has integrated certificate validation for TLS streams, you may consider reusing that.

Consumers from C or C++ most likely do not need to do this.

#### Parameters

 * `uri`: `IN`: The `mongoc_uri_t` describing our connection.
 * `host`: `IN`: The idividual host we want to connect to.
 * `user_data`: `IN`: User data provided to `mongoc_client_set_stream_initiator()`.
 * `error`: `OUT`: A location of a `bson_error_t` or `NULL`.

#### Returns

A newly created `mongoc_stream_` if successful; otherwise `NULL` and `error` is set.

**Availability**: Since 0.90.0

----------

## Thread Safety

`mongoc_client_t` is **NOT** thread-safe and should only be used from one thread at a time. When used in multi-threaded scenarios, it is recommended that you use `mongoc_client_pool_t` to retrieve a `mongoc_client_t` for your thread.

----------

## Lifecycle

It is an error to call `mongoc_client_destroy()` on a client that has operations pending. It is required that you release `mongoc_collection_t` and `mongoc_database_t` structures before calling `mongoc_client_destroy()`.

----------

## Examples

The following example connects to a single MongoDB instance and performs a simple query against it. The resulting documents are printed as `JSON` to standard output.

```c
/* gcc example.c -o example $(pkg-config --cflags --libs libmongoc-1.0) */

#include <mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc,
      char *argv[])
{
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    bson_error_t error;
    const bson_t *doc;
    bson_t query;
    char *str;
    
    client = mongoc_client_new("mongodb://localhost:27017/");
    if (!client) {
        fprintf(stderr, "Failed to parse URI.\n");
        return EXIT_FAILURE;
    }
    
    bson_init(&query);
    bson_append_utf8(&query, "hello", -1, "world", -1);
    
    
    collection = mongoc_client_get_collection(client, "test", "test");
    cursor = mongoc_collection_find(collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    0,
                                    &query,
                                    NULL,  /* Fields, NULL for all. */
                                    NULL); /* Read Prefs, NULL for default */
    
    while (mongoc_cursor_next(cursor, &doc)) {
        str = bson_as_json(doc, NULL);
        fprintf(stdout, "%s\n", str);
        bson_free(str);
    }
    
    if (mongoc_cursor_error(cursor, &error)) {
        fprintf(stderr, "Cursor Failure: %s\n", error.message);
    }
    
    bson_destroy(&query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    
    return EXIT_SUCCESS;
}
```
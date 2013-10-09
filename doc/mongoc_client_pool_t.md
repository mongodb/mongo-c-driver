# mongoc_client_pool_t

```c
typedef struct _mongoc_client_pool_t mongoc_client_pool_t;
```

`mongoc_client_pool_t` is an opaque structure providing a traditional connection pool design around the `mongoc_client_t` structure.

---------

## Constructors

### mongoc_client_pool_new

```c
mongoc_client_pool_t *
mongoc_client_pool_new (const mongoc_uri_t *uri);
```

Creates a new `mongoc_client_pool_t` using the provided `mongoc_uri_t`. The uri will not be modified.

#### Parameters

 * `uri`: `IN`: A `mongoc_uri_t` of the server, replica set, or sharded cluster.

#### Returns

A newly allocated `mongoc_client_pool_t` that should be freed with `mongoc_client_pool_destroy()` when no longer in use.

#### Availability

Available since version 0.2.0.

----------

## Destructors

### mongoc_client_pool_destroy

```c
void
mongoc_client_pool_destroy (mongoc_client_pool_t *client_pool);
```

Destroys a `mongoc_client_pool_t` that is no longer in use. This function will release any resources currently in use by `client_pool`.

It is a programming error to use `client_pool` after calling this function.

#### Parameters

 * `client_pool`: `IN`: A `mongoc_client_pool_t` to destroy.

#### Availability

Available since version 0.2.0.

-----------

## Functions

### mongoc_client_pool_pop

```c
mongoc_client_t *
mongoc_client_pool_pop (mongoc_client_pool_t *client_pool);
```

Retrieves a `mongoc_client_t` from the `mongoc_client_pool_t`. If no clients are available for reuse, then a new `mongoc_client_t` will be created.

If the client pool has reached it's maximum number of connections, this function will block until a client has been returned back to the pool.

#### Parameters

 * `client_pool`: `IN`: A `mongoc_client_pool_t` to fetch a client from.

#### Returns

A `mongoc_client_t`. It should be released with `mongoc_client_pool_push()` when you have finished using it. It is a programming error to call `mongoc_client_destroy()`.

#### Availability

Available since version 0.2.0.

---------

### mongoc_client_pool_push

```c
void
mongoc_client_pool_push (mongoc_client_pool_t *client_pool,
                         mongoc_client_t      *client);
```

This function returns `client` back into `client_pool`. If another thread is currently waiting on `mongoc_client_pool_pop()` it will be notified and receive `client`, or alternatively, a newly created `mongoc_client_t`.

It is a programming error to use `client` after calling this function.

#### Parameters

 * `client_pool`: `IN`: A `mongoc_client_pool_t`.
 * `client`: `IN`: A `mongoc_client_t` retrieved from `mongoc_client_pool_pop()`.

#### Availability

Available since version 0.2.0.

---------

## Thread Safety

`mongoc_client_pool_t` is thread-safe. You may use it from multiple threads so long as `mongoc_client_pool_destroy()` has not been called.

--------

## Lifecycle

It is a programming error to release the `mongoc_client_pool_t` before pushing the `mongoc_client_t` back into the pool.

----------

## Examples

```c
#include <mongoc.h>

int
main (int argc,
      char *argv[])
{
   mongoc_client_pool_t *client_pool;
   mongoc_uri_t *uri;
   
   uri = mongoc_uri_new("mongodb://localhost:27017/");
   client_pool = mongoc_client_pool_new(uri);
   
   /* You may consider doing this in a worker thread. */
   client = mongoc_client_pool_pop(client_pool);
   do_something(client);
   mongoc_client_pool_push(client_pool, client);
   
   mongoc_client_pool_destroy(client_pool);
   mongoc_uri_destroy(uri);

   return 0;
}
```
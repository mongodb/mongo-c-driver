# mongoc_cursor_t

```c
typedef struct _mongoc_cursor_t mongoc_cursor_t;
```

`mongoc_cursor_t` is an opaque structure providing management of MongoDB server side cursors. You can iterate through the results of the cursors and access the individual documents returned.

----------

## Destructors

----------

## Functions 

### mongoc_cursor_next

```c
bson_bool_t
mongoc_cursor_next (mongoc_cursor_t  *cursor,
                    const bson_t    **bson);
```

Moves the cursor to the next document in the result set. Results are fetched from the MongoDB server in batches. If there are no documents left in the current batch, a suplimental query to the server will be performed.

If there was a failure or the result set has been exhausted, `FALSE` is returned.

You can check for failure by calling `mongoc_cursor_error()` when this returns `FALSE`.

The resulting `bson_t` is owned by `cursor` and therefore **MUST NOT** be modified or freed. Additionally, `bson` is only valid until the next call to any function on `cursor`.

#### Parameters

 * `cursor`: `IN`: A `mongoc_cursor_t`.
 * `bson`: `OUT`: A location for a `bson_t`.

#### Returns

`TRUE` and `bson` is set if successful; otherwise `FALSE`. You may check the error result with `mongoc_cursor_error()`.

**Since**: 0.90.0

----------

### mongoc_cursor_error

```c
bson_bool_t
mongoc_cursor_error (mongoc_cursor_t *cursor,
                     bson_error_t    *error);
```

This function checks to see if an error occurred on the cursor. If so, `TRUE` is returned and `error` is set. If no error has occurred, `FALSE` is returned and `error` is not modified.

#### Parameters

 * `cursor`: `IN`: A `mongoc_cursor_t`.
 * `error`: `OUT`: A `bson_error_t` that will be set if there was an error.

#### Returns

`FALSE` if there has been no error; otherwise `TRUE` and `error` is set.

**Since**: 0.90.0

----------

## Thread Safety

`mongoc_cursor_t` is not thread-safe and should never be shared between threads.

----------

## Lifecycle

`mongoc_cursor_t` should be released before releasing the creating structure such as `mongoc_collection_t` or `mongoc_database_t`.

----------

## Examples

```c
static void
exhaust_cursor_and_free (mongoc_cursor_t *cursor)
{
    bson_error_t error;
    const bson_t *doc;
    char *str;
    
    while (mongoc_cursor_next(cursor, &doc)) {
        str = bson_as_json(doc, NULL);
        printf("%s\n", str);
        bson_free(str);
    }
    
    if (mongoc_cursor_error(cursor, &error)) {
        printf("Cursor Failure: %s\n", error.message);
    }
    
    mongoc_cursor_destroy(cursor);
}
```

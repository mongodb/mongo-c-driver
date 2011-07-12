# MongoDB C Driver History

## 0.4 (UNRELEASED)

THIS RELEASE INCLUDES NUMEROUS BACKWARD-BREAKING CHANGES.
These changes have been made for extensibility, consistency,
and ease of use. Please read the following release notes
carefully, and study the updated tutorial.

* mongo_replset_init_conn has been renamed to mongo_replset_init.
* bson_buffer has been removed. All methods for building bson
  objects now take objects of type bson. The new pattern looks like this:

    bson b[1];
    bson_init( b );
    bson_append_int( b, "foo", 1 );
    bson_finish( b );
    /* The object is ready to use. */
    bson_destroy( b );

* mongo_connection has been renamed to mongo.
* All constants that were once lower case are now
  upper case. These include: MONGO_OP_MSG, MONGO_OP_UPDATE, MONGO_OP_INSERT,
  MONGO_OP_QUERY, MONGO_OP_GET_MORE, MONGO_OP_DELETE, MONGO_OP_KILL_CURSORS
  BSON_EOO, BSON_DOUBLE, BSON_STRING, BSON_OBJECT, BSON_ARRAY, BSON_BINDATA,
  BSON_UNDEFINED, BSON_OID, BSON_BOOL, BSON_DATE, BSON_NULL, BSON_REGEX, BSON_DBREF,
  BSON_CODE, BSON_SYMBOL, BSON_CODEWSCOPE, BSON_INT, BSON_TIMESTAMP, BSON_LONG,
  MONGO_CONN_SUCCESS, MONGO_CONN_BAD_ARG, MONGO_CONN_NO_SOCKET, MONGO_CONN_FAIL,
  MONGO_CONN_NOT_MASTER, MONGO_CONN_BAD_SET_NAME, MONGO_CONN_CANNOT_FIND_PRIMARY 
  If your programs use any of these constants, you must convert them to their
  upper case forms, or you will see compile errors.
* Methods taking a mongo_connection object now return either MONGO_OK or MONGO_ERROR.
  In case of an error, an error code of type mongo_error_t will be indicated on the
  mongo_connection->err field.
* Methods taking a bson object now return either BSON_OK or BSON_ERROR.
  In case of an error, an error code of type bson_validity_t will be indicated on the
  bson->err or bson_buffer->err field.
* Calls to mongo_cmd_get_last_error store the error status on the
  mongo->lasterrcode and mongo->lasterrstr.
* Fixed a few memory leaks.

## 0.3
2011-4-14

* Support replica sets.
* Better standard connection API.
* GridFS write buffers iteratively.
* Fixes for working with large GridFS files (> 3GB)
* bson_append_string_n and family (Gergely Nagy)

## 0.2
2011-2-11

* GridFS support (Chris Triolo).
* BSON Timestamp type support.

## 0.1
2009-11-30

* Initial release.

# MongoDB C Driver History

## 0.4 (UNRELEASED)

* All constants that were once lower case are now
  upper case. These include: MONGO_OP_MSG, MONGO_OP_UPDATE, MONGO_OP_INSERT,
  MONGO_OP_QUERY, MONGO_OP_GET_MORE, MONGO_OP_DELETE, MONGO_OP_KILL_CURSORS
  BSON_EOO, BSON_DOUBLE, BSON_STRING, BSON_OBJECT, BSON_ARRAY, BSON_BINDATA,
  BSON_UNDEFINED, BSON_OID, BSON_BOOL, BSON_DATE, BSON_NULL, BSON_REGEX, BSON_DBREF,
  BSON_CODE, BSON_SYMBOL, BSON_CODEWSCOPE, BSON_INT, BSON_TIMESTAMP, BSON_LONG,
  If your programs use any of these constants, you must convert them to their
  upper case forms, or you will see compile errors.
* Methods taking a mongo_connection object now return either MONGO_OK or MONGO_ERROR.
  In case of an error, an error code of type mongo_error_t will be indicated on the
  mongo_connection->err field.
* Methods taking a bson or bson_buffer object now return either BSON_OK or BSON_ERROR.
  In case of an error, an error code of type bson_validity_t will be indicated on the
  bson->err or bson_buffer->err field.
* Calls to mongo_cmd_get_last_error store error status on mongo_connection->lasterrcode and
  mongo_connection->lasterrstr.
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

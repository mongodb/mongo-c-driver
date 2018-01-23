#include "mongoc-opts-helpers.h"
#include "mongoc-util.c"
#include "mongoc-client-session-private.h"
#include "mongoc-write-concern-private.h"

bool
_mongoc_convert_document (const bson_iter_t *iter, bson_t *doc, bson_error_t *error) {
   //check that iter holds document
   if (BSON_ITER_HOLDS_DOCUMENT(iter)) {
      //copy contents of iter to stage
      bson_value_copy(bson_iter_value(iter), doc);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid 'opts' parameter.");
      return false;
   }
}

bool
_mongoc_convert_int64_positive (const bson_iter_t *iter, int64_t *num, bson_error_t *error) {
   //check that iter holds an int64
   if (BSON_ITER_HOLDS_INT64(iter) && bson_iter_int64(iter) >= 0x0) {
      //copy contents of iter to num
      *num = bson_iter_int64 (iter);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid 'opts' parameter.");
      return false;
   }
}

bool
_mongoc_convert_bool (const bson_iter_t *iter, bool *flag, bson_error_t *error) {
   //check that iter holds a bool
   if (BSON_ITER_HOLDS_BOOL(iter)) {
      //copy contents of iter to flag
      *flag = bson_iter_bool (iter);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid 'opts' parameter.");
      return false;
   }
}

bool
_mongoc_convert_bson_value_t (const bson_iter_t *iter, bson_value_t *value, bson_error_t *error) {
   bson_value_copy(bson_iter_value(iter), value);
   return true;
}

bool
_mongoc_convert_utf8 (const bson_iter_t *iter, const char **str, bson_error_t *error) {
   //check that iter holds a utf8
   if (BSON_ITER_HOLDS_UTF8(iter)) {
      *str = bson_iter_utf8(iter, NULL);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid 'opts' parameter.");
      return false;
   }
}

bool
_mongoc_convert_write_concern (const bson_iter_t *iter, mongoc_write_concern_t *wc, bson_error_t *error) {

   mongoc_write_concern_t *temp;
   temp = _mongoc_write_concern_new_from_iter (iter, error);
   if (!temp) {
      return false;
   } else {
      mongoc_write_concern_copy_to(temp, wc);
      return true;
   }
}

bool
_mongoc_convert_session_id (const bson_iter_t *iter, mongoc_client_session_t *session, bson_error_t *error, mongoc_collection_t *collection) {
   if (!_mongoc_client_session_from_iter (
      collection->client, iter, &session, error)) {
      return false;
   }
   return true;
}


bool
_mongoc_convert_validate_flags (const bson_iter_t *iter, bson_validate_flags_t *flags, bson_error_t *error) {
   if (BSON_ITER_HOLDS_BOOL (iter)) {
      if (!bson_iter_as_bool (iter)) {
         *flags = 0;
         return true;
      } else {
         return false;
      }
   } else if (BSON_ITER_HOLDS_INT32 (iter)) {
      if (bson_iter_int32 (iter) <= 0x1F) {
         *flags = bson_iter_int32 (iter);
         return true;
      } else {
         return false;
      }
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Invalid type for option \"validate\", \"%s\":"
                         " \"validate\" must be a bitwise-or of all desired "
                         "bson_validate_flags_t.",
                      _mongoc_bson_type_to_str (bson_iter_type (iter)));
      return false;
   }
}

bool
_mongoc_convert_bypass_doc_eval(const bson_iter_t *iter, mongoc_write_bypass_document_validation_t *bdv, bson_error_t *error) {
   if (BSON_ITER_HOLDS_BOOL (iter)) {
      if (bson_iter_bool (iter) == true) {
         *bdv = MONGOC_BYPASS_DOCUMENT_VALIDATION_TRUE;
      } else {
         *bdv = MONGOC_BYPASS_DOCUMENT_VALIDATION_FALSE;
      }
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid 'opts' parameter.");
      return false;
   }
}

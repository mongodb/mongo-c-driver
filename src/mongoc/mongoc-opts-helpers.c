#include "mongoc-opts-helpers-private.h"
#include "mongoc-client-session-private.h"
#include "mongoc-write-concern-private.h"
#include "mongoc-util-private.h"


bool
_mongoc_convert_document (mongoc_client_t *client,
                          const bson_iter_t *iter,
                          bson_t *doc,
                          bson_error_t *error)
{
   uint32_t len;
   const uint8_t *data;
   bson_t value;

   if (BSON_ITER_HOLDS_DOCUMENT (iter)) {
      bson_iter_document (iter, &len, &data);
      bson_init_static (&value, data, len);
      bson_copy_to (&value, doc);
      bson_destroy (&value);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid field \"%s\" in opts",
                      bson_iter_key (iter));
      return false;
   }
}

bool
_mongoc_convert_int64_positive (mongoc_client_t *client,
                                const bson_iter_t *iter,
                                int64_t *num,
                                bson_error_t *error)
{
   if (BSON_ITER_HOLDS_INT64 (iter) && bson_iter_int64 (iter) >= 0) {
      *num = bson_iter_int64 (iter);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid field \"%s\" in opts",
                      bson_iter_key (iter));
      return false;
   }
}

bool
_mongoc_convert_bool (mongoc_client_t *client,
                      const bson_iter_t *iter,
                      bool *flag,
                      bson_error_t *error)
{
   if (BSON_ITER_HOLDS_BOOL (iter)) {
      *flag = bson_iter_bool (iter);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid field \"%s\" in opts",
                      bson_iter_key (iter));
      return false;
   }
}

bool
_mongoc_convert_bson_value_t (mongoc_client_t *client,
                              const bson_iter_t *iter,
                              bson_value_t *value,
                              bson_error_t *error)
{
   bson_value_copy (bson_iter_value ((bson_iter_t *) iter), value);
   return true;
}

bool
_mongoc_convert_utf8 (mongoc_client_t *client,
                      const bson_iter_t *iter,
                      const char **str,
                      bson_error_t *error)
{
   if (BSON_ITER_HOLDS_UTF8 (iter)) {
      *str = bson_iter_utf8 (iter, NULL);
      return true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "Invalid field \"%s\" in opts",
                      bson_iter_key (iter));
      return false;
   }
}

bool
_mongoc_convert_validate_flags (mongoc_client_t *client,
                                const bson_iter_t *iter,
                                bson_validate_flags_t *flags,
                                bson_error_t *error)
{
   if (BSON_ITER_HOLDS_BOOL (iter)) {
      if (!bson_iter_as_bool (iter)) {
         *flags = BSON_VALIDATE_NONE;
         return true;
      } else {
         /* validate: false is ok but validate: true is prohibited */
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Invalid option \"%s\": true, must be a bitwise-OR of"
                         " bson_validate_flags_t values.",
                         bson_iter_key (iter));
         return false;
      }
   } else if (BSON_ITER_HOLDS_INT32 (iter)) {
      if (bson_iter_int32 (iter) <= 0x1F) {
         *flags = (bson_validate_flags_t) bson_iter_int32 (iter);
         return true;
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Invalid field \"%s\" in opts, must be a bitwise-OR of"
                         " bson_validate_flags_t values.",
                         bson_iter_key (iter));
         return false;
      }
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Invalid type for option \"%s\": \"%s\"."
                      " \"%s\" must be a a boolean or a bitwise-OR of"
                      " bson_validate_flags_t values.",
                      bson_iter_key (iter),
                      _mongoc_bson_type_to_str (bson_iter_type (iter)),
                      bson_iter_key (iter));
      return false;
   }
}

bool
_mongoc_convert_mongoc_write_bypass_document_validation_t (
   mongoc_client_t *client,
   const bson_iter_t *iter,
   mongoc_write_bypass_document_validation_t *bdv,
   bson_error_t *error)
{
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
                      "Invalid field \"%s\" in opts",
                      bson_iter_key (iter));
      return false;
   }
}

bool
_mongoc_convert_write_concern (mongoc_client_t *client,
                               const bson_iter_t *iter,
                               mongoc_write_concern_t **wc,
                               bson_error_t *error)
{
   mongoc_write_concern_t *tmp;

   tmp = _mongoc_write_concern_new_from_iter (iter, error);
   if (tmp) {
      *wc = tmp;
      return true;
   }

   return false;
}

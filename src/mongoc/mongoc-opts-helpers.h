#include <bson.h>
#include "mongoc-client-session.h"
#include "mongoc-collection-private.h"
#include "mongoc-write-command-private.h"

#ifndef LIBMONGOC_MONGOC_OPTS_HELPERS_H
#define LIBMONGOC_MONGOC_OPTS_HELPERS_H

bool
_mongoc_convert_document (const bson_iter_t *iter, bson_t *doc, bson_error_t *error);

bool
_mongoc_convert_int64_positive (const bson_iter_t *iter, int64_t *num, bson_error_t *error);

bool
_mongoc_convert_bool (const bson_iter_t *iter, bool *flag, bson_error_t *error);

bool
_mongoc_convert_bson_value_t (const bson_iter_t *iter, bson_value_t *value, bson_error_t *error);

bool
_mongoc_convert_utf8 (const bson_iter_t *iter, const char **comment, bson_error_t *error);

bool
_mongoc_convert_write_concern (const bson_iter_t *iter, mongoc_write_concern_t *wc, bson_error_t *error);

bool
_mongoc_convert_session_id (const bson_iter_t *iter, mongoc_client_session_t *session, bson_error_t *error, mongoc_collection_t *collection);

bool
_mongoc_convert_validate_flags (const bson_iter_t *iter, bson_validate_flags_t *flags, bson_error_t *error);

bool
_mongoc_convert_bypass_doc_eval(const bson_iter_t *iter, mongoc_write_bypass_document_validation_t *bdv, bson_error_t *error);



#endif //LIBMONGOC_MONGOC_OPTS_HELPERS_H

#include "mongoc-matcher-private.h"

static mongoc_matcher_op_t *
_mongoc_matcher_parse_logical (mongoc_matcher_opcode_t  opcode,
                               bson_iter_t             *iter,
                               bson_bool_t              is_root);

static mongoc_matcher_op_t *
_mongoc_matcher_parse_compare(bson_iter_t * iter, const char * path)
{
   const char * key;
   mongoc_matcher_op_t * op = NULL, * op_child;
   bson_iter_t child;
   bson_bool_t r;

   if (bson_iter_type(iter) == BSON_TYPE_DOCUMENT) {
      bson_iter_recurse(iter, &child);
      r = bson_iter_next(&child);

      assert(r);

      key = bson_iter_key(&child);

      if (key[0] != '$') {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_EQ, path, iter);
      } else if (strcmp(key, "$not") == 0) {
         op_child = _mongoc_matcher_parse_compare(&child, path);
         op = mongoc_matcher_op_not_new(path, op_child);
      } else if (strcmp(key, "$gt") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_GT, path, &child);
      } else if (strcmp(key, "$gte") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_GTE, path, &child);
      } else if (strcmp(key, "$in") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_IN, path, &child);
      } else if (strcmp(key, "$lt") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_LT, path, &child);
      } else if (strcmp(key, "$lte") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_LTE, path, &child);
      } else if (strcmp(key, "$ne") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_NE, path, &child);
      } else if (strcmp(key, "$nin") == 0) {
         op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_NIN, path, &child);
      } else if (strcmp(key, "$exists") == 0) {
         op = mongoc_matcher_op_exists_new(path, bson_iter_bool(&child));
      } else if (strcmp(key, "$type") == 0) {
         op = mongoc_matcher_op_type_new(path, bson_iter_type(&child));
      } else {
         fprintf(stderr, "Invalid operator '%s'. Unknown or invalid position\n", key);
         abort();
      }
   } else {
      op = mongoc_matcher_op_compare_new(MONGOC_MATCHER_OPCODE_EQ, path, iter);
   }

   return op;
}

static mongoc_matcher_op_t *
_mongoc_matcher_parse(bson_iter_t * iter)
{
   bson_iter_t child;
   mongoc_matcher_op_t * op;
   const char * key;
   bson_bool_t r;

   key = bson_iter_key(iter);

   if (key[0] != '$') {
      op = _mongoc_matcher_parse_compare(iter, key);
      return op;
   } else {
      assert(bson_iter_type(iter) == BSON_TYPE_ARRAY);
      r = bson_iter_recurse(iter, &child);
      assert(r);

      if (strcmp(key, "$or") == 0) {
         return _mongoc_matcher_parse_logical(MONGOC_MATCHER_OPCODE_OR, &child, FALSE);
      } else if (strcmp(key, "$and") == 0) {
         return _mongoc_matcher_parse_logical(MONGOC_MATCHER_OPCODE_AND, &child, FALSE);
      } else if (strcmp(key, "$nor") == 0) {
         return _mongoc_matcher_parse_logical(MONGOC_MATCHER_OPCODE_NOR, &child, FALSE);
      } else {
         fprintf(stderr, "Invalid operator '%s'. Unknown or invalid position\n", key);
         abort();
      }
   }
}

static mongoc_matcher_op_t *
_mongoc_matcher_parse_logical (mongoc_matcher_opcode_t  opcode,
                               bson_iter_t             *iter,
                               bson_bool_t              is_root)
{
   mongoc_matcher_op_t * left, * right, * more, * more_wrap;
   bson_bool_t r;
   bson_iter_t child;

   r = bson_iter_next(iter);
   if (! r) { return NULL; };

   if (is_root) {
      left = _mongoc_matcher_parse(iter);
   } else {
      assert(bson_iter_type(iter) == BSON_TYPE_DOCUMENT);

      bson_iter_recurse(iter, &child);

      left = _mongoc_matcher_parse(&child);
   }

   r = bson_iter_next(iter);
   if (! r) { return left; };

   if (is_root) {
      right = _mongoc_matcher_parse(iter);
   } else {
      assert(bson_iter_type(iter) == BSON_TYPE_DOCUMENT);

      bson_iter_recurse(iter, &child);

      right = _mongoc_matcher_parse(&child);
   }

   more = _mongoc_matcher_parse_logical(opcode, iter, is_root);

   if (more) {
      more_wrap = mongoc_matcher_op_logical_new(opcode, right, more);

      return mongoc_matcher_op_logical_new(opcode, left, more_wrap);
   } else {
      return mongoc_matcher_op_logical_new(opcode, left, right);
   }
}

mongoc_matcher_t *
mongoc_matcher_new (const bson_t *query)
{
   bson_iter_t iter;
   bson_bool_t r;
   mongoc_matcher_op_t * op;
   mongoc_matcher_t * matcher;
   
   r = bson_iter_init(&iter, query);

   if (!r) return NULL;

   op = _mongoc_matcher_parse_logical(MONGOC_MATCHER_OPCODE_AND, &iter, TRUE);

   matcher = calloc(sizeof *matcher, 1);
   bson_copy_to(query, &matcher->query);

   matcher->optree = op;

   return matcher;
}

void
mongoc_matcher_destroy(mongoc_matcher_t * matcher)
{
   mongoc_matcher_op_free(matcher->optree);
   bson_destroy(&matcher->query);
   free(matcher);
}

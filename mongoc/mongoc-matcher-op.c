/*
 * Copyright 2014 10gen Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "mongoc-matcher-op-private.h"


mongoc_matcher_op_t *
mongoc_matcher_op_exists_new (const char *path,
                              bson_bool_t exists)
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (path);

   op = bson_malloc0 (sizeof *op);
   op->exists.base.opcode = MONGOC_MATCHER_OPCODE_EXISTS;
   op->exists.path = bson_strdup (path);
   op->exists.exists = exists;

   return op;
}


mongoc_matcher_op_t *
mongoc_matcher_op_type_new (const char *path,
                            bson_type_t type)
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (path);
   BSON_ASSERT (type);

   op = bson_malloc0 (sizeof *op);
   op->type.base.opcode = MONGOC_MATCHER_OPCODE_TYPE;
   op->type.path = bson_strdup (path);
   op->type.type = type;

   return op;
}


mongoc_matcher_op_t *
mongoc_matcher_op_logical_new (mongoc_matcher_opcode_t opcode,
                               mongoc_matcher_op_t *left,
                               mongoc_matcher_op_t *right)
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (left);
   BSON_ASSERT ((opcode >= MONGOC_MATCHER_OPCODE_OR) &&
                (opcode <= MONGOC_MATCHER_OPCODE_NOR));

   op = bson_malloc0 (sizeof *op);
   op->logical.base.opcode = opcode;
   op->logical.left = left;
   op->logical.right = right;

   return op;
}


mongoc_matcher_op_t *
mongoc_matcher_op_compare_new (mongoc_matcher_opcode_t opcode,
                               const char *path,
                               const bson_iter_t *iter)
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT ((opcode >= MONGOC_MATCHER_OPCODE_EQ) &&
                (opcode <= MONGOC_MATCHER_OPCODE_NIN));
   BSON_ASSERT (path);
   BSON_ASSERT (iter);

   op = bson_malloc0 (sizeof *op);
   op->compare.base.opcode = opcode;
   op->compare.path = bson_strdup (path);
   memcpy (&op->compare.iter, iter, sizeof *iter);

   return op;
}


mongoc_matcher_op_t *
mongoc_matcher_op_not_new (const char *path,
                           mongoc_matcher_op_t *child)
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (path);
   BSON_ASSERT (child);

   op = bson_malloc0 (sizeof *op);
   op->not.base.opcode = MONGOC_MATCHER_OPCODE_NOT;
   op->not.path = bson_strdup (path);
   op->not.child = child;

   return op;
}


void
mongoc_matcher_op_free (mongoc_matcher_op_t *op)
{
   BSON_ASSERT (op);

   switch (op->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
   case MONGOC_MATCHER_OPCODE_GT:
   case MONGOC_MATCHER_OPCODE_GTE:
   case MONGOC_MATCHER_OPCODE_IN:
   case MONGOC_MATCHER_OPCODE_LT:
   case MONGOC_MATCHER_OPCODE_LTE:
   case MONGOC_MATCHER_OPCODE_NE:
   case MONGOC_MATCHER_OPCODE_NIN:
      bson_free (op->compare.path);
      break;
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOR:
      if (op->logical.left)
         mongoc_matcher_op_free (op->logical.left);
      if (op->logical.right)
         mongoc_matcher_op_free (op->logical.right);
      break;
   case MONGOC_MATCHER_OPCODE_NOT:
      mongoc_matcher_op_free (op->not.child);
      bson_free (op->not.path);
      break;
   case MONGOC_MATCHER_OPCODE_EXISTS:
      bson_free (op->exists.path);
      break;
   case MONGOC_MATCHER_OPCODE_TYPE:
      bson_free (op->type.path);
      break;
   default:
      break;
   }

   bson_free (op);
}


static bson_bool_t
mongoc_matcher_op_exists_match (mongoc_matcher_op_exists_t *exists,
                                const bson_t *bson)
{
   bson_iter_t iter;
   bson_iter_t desc;
   bson_bool_t found;

   BSON_ASSERT (exists);
   BSON_ASSERT (bson);

   found = (bson_iter_init (&iter, bson) &&
            bson_iter_find_descendant (&iter, exists->path, &desc));

   return (found == exists->exists);
}


static bson_bool_t
mongoc_matcher_op_type_match (mongoc_matcher_op_type_t *type,
                              const bson_t *bson)
{
   bson_iter_t iter;
   bson_iter_t desc;

   BSON_ASSERT (type);
   BSON_ASSERT (bson);

   if (bson_iter_init (&iter, bson) &&
       bson_iter_find_descendant (&iter, type->path, &desc)) {
      return (bson_iter_type (&iter) == type->type);
   }

   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_not_match (mongoc_matcher_op_not_t *not,
                             const bson_t *bson)
{
   BSON_ASSERT (not);
   BSON_ASSERT (bson);

   return !mongoc_matcher_op_match (not->child, bson);
}


#define _TYPE_CODE(l, r) ((((int)(l)) << 8) | ((int)(r)))
#define _EQ_COMPARE(t1,t2) \
   (bson_iter_##t1(&compare->iter) == bson_iter_##t2(iter))


static bson_bool_t
mongoc_matcher_op_eq_match (mongoc_matcher_op_compare_t *compare,
                            bson_iter_t *iter)
{
   int leftcode = bson_iter_type (&compare->iter);
   int rightcode = bson_iter_type (iter);
   int eqcode;

   eqcode = (leftcode << 8) | rightcode;

   switch (eqcode) {

   /* Double on Left Side */
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_DOUBLE):
      return _EQ_COMPARE (double, double);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_BOOL):
      return _EQ_COMPARE (double, bool);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT32):
      return _EQ_COMPARE (double, int32);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT64):
      return _EQ_COMPARE (double, int64);

   /* UTF8 on Left Side */
   case _TYPE_CODE(BSON_TYPE_UTF8, BSON_TYPE_UTF8):
      {
         bson_uint32_t llen;
         bson_uint32_t rlen;
         const char *lstr;
         const char *rstr;

         lstr = bson_iter_utf8 (&compare->iter, &llen);
         rstr = bson_iter_utf8 (iter, &rlen);

         return ((llen == rlen) && (0 == memcmp (lstr, rstr, llen)));
      }

   /* Int32 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_DOUBLE):
      return _EQ_COMPARE (int32, double);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_BOOL):
      return _EQ_COMPARE (int32, bool);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT32):
      return _EQ_COMPARE (int32, int32);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT64):
      return _EQ_COMPARE (int32, int64);

   /* Int64 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_DOUBLE):
      return _EQ_COMPARE (int64, double);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_BOOL):
      return _EQ_COMPARE (int64, bool);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT32):
      return _EQ_COMPARE (int64, int32);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT64):
      return _EQ_COMPARE (int64, int64);

   default:
      break;
   }

   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_gt_match (mongoc_matcher_op_compare_t *compare,
                            bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_gte_match (mongoc_matcher_op_compare_t *compare,
                             bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_in_match (mongoc_matcher_op_compare_t *compare,
                            bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_lt_match (mongoc_matcher_op_compare_t *compare,
                            bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_lte_match (mongoc_matcher_op_compare_t *compare,
                             bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_ne_match (mongoc_matcher_op_compare_t *compare,
                            bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_nin_match (mongoc_matcher_op_compare_t *compare,
                             bson_iter_t *iter)
{
   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_compare_match (mongoc_matcher_op_compare_t *compare,
                                 const bson_t *bson)
{
   bson_iter_t iter;

   BSON_ASSERT (compare);
   BSON_ASSERT (bson);

   if (!bson_iter_init_find (&iter, bson, compare->path)) {
      return FALSE;
   }

   switch ((int)compare->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
      return mongoc_matcher_op_eq_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_GT:
      return mongoc_matcher_op_gt_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_GTE:
      return mongoc_matcher_op_gte_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_IN:
      return mongoc_matcher_op_in_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_LT:
      return mongoc_matcher_op_lt_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_LTE:
      return mongoc_matcher_op_lte_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_NE:
      return mongoc_matcher_op_ne_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_NIN:
      return mongoc_matcher_op_nin_match (compare, &iter);
   default:
      BSON_ASSERT (FALSE);
      break;
   }

   return FALSE;
}


static bson_bool_t
mongoc_matcher_op_logical_match (mongoc_matcher_op_logical_t *logical,
                                 const bson_t *bson)
{
   BSON_ASSERT (logical);
   BSON_ASSERT (bson);

   switch ((int)logical->base.opcode) {
   case MONGOC_MATCHER_OPCODE_OR:
      return (mongoc_matcher_op_match (logical->left, bson) ||
              mongoc_matcher_op_match (logical->right, bson));
   case MONGOC_MATCHER_OPCODE_AND:
      return (mongoc_matcher_op_match (logical->left, bson) &&
              mongoc_matcher_op_match (logical->right, bson));
   case MONGOC_MATCHER_OPCODE_NOR:
      return !(mongoc_matcher_op_match (logical->left, bson) ||
               mongoc_matcher_op_match (logical->right, bson));
   default:
      BSON_ASSERT (FALSE);
      break;
   }

   return FALSE;
}


bson_bool_t
mongoc_matcher_op_match (mongoc_matcher_op_t *op,
                         const bson_t *bson)
{
   BSON_ASSERT (op);
   BSON_ASSERT (bson);

   switch (op->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
   case MONGOC_MATCHER_OPCODE_GT:
   case MONGOC_MATCHER_OPCODE_GTE:
   case MONGOC_MATCHER_OPCODE_IN:
   case MONGOC_MATCHER_OPCODE_LT:
   case MONGOC_MATCHER_OPCODE_LTE:
   case MONGOC_MATCHER_OPCODE_NE:
   case MONGOC_MATCHER_OPCODE_NIN:
      return mongoc_matcher_op_logical_match (&op->logical, bson);
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOR:
      return mongoc_matcher_op_compare_match (&op->compare, bson);
   case MONGOC_MATCHER_OPCODE_NOT:
      return mongoc_matcher_op_not_match (&op->not, bson);
   case MONGOC_MATCHER_OPCODE_EXISTS:
      return mongoc_matcher_op_exists_match (&op->exists, bson);
   case MONGOC_MATCHER_OPCODE_TYPE:
      return mongoc_matcher_op_type_match (&op->type, bson);
   default:
      break;
   }

   return FALSE;
}


void
mongoc_matcher_op_to_bson (mongoc_matcher_op_t *op,
                           bson_t *bson)
{
   const char *str;
   bson_t child;
   bson_t child2;

   BSON_ASSERT (op);
   BSON_ASSERT (bson);

   switch (op->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
      bson_append_iter (bson, op->compare.path, -1, &op->compare.iter);
      break;
   case MONGOC_MATCHER_OPCODE_GT:
   case MONGOC_MATCHER_OPCODE_GTE:
   case MONGOC_MATCHER_OPCODE_IN:
   case MONGOC_MATCHER_OPCODE_LT:
   case MONGOC_MATCHER_OPCODE_LTE:
   case MONGOC_MATCHER_OPCODE_NE:
   case MONGOC_MATCHER_OPCODE_NIN:
      switch ((int)op->base.opcode) {
      case MONGOC_MATCHER_OPCODE_GT:
         str = "$gt";
         break;
      case MONGOC_MATCHER_OPCODE_GTE:
         str = "$gte";
         break;
      case MONGOC_MATCHER_OPCODE_IN:
         str = "$in";
         break;
      case MONGOC_MATCHER_OPCODE_LT:
         str = "$lt";
         break;
      case MONGOC_MATCHER_OPCODE_LTE:
         str = "$lte";
         break;
      case MONGOC_MATCHER_OPCODE_NE:
         str = "$ne";
         break;
      case MONGOC_MATCHER_OPCODE_NIN:
         str = "$nin";
         break;
      default:
         break;
      }
      bson_append_document_begin (bson, op->compare.path, -1, &child);
      bson_append_iter (&child, str, -1, &op->compare.iter);
      bson_append_document_end (bson, &child);
      break;
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOR:
      if (op->base.opcode == MONGOC_MATCHER_OPCODE_OR) {
         str = "$or";
      } else if (op->base.opcode == MONGOC_MATCHER_OPCODE_AND) {
         str = "$and";
      } else if (op->base.opcode == MONGOC_MATCHER_OPCODE_NOR) {
         str = "$nor";
      } else {
         BSON_ASSERT (FALSE);
         str = NULL;
      }
      bson_append_array_begin (bson, str, -1, &child);
      bson_append_document_begin (&child, "0", 1, &child2);
      mongoc_matcher_op_to_bson (op->logical.left, &child2);
      bson_append_document_end (&child, &child2);
      if (op->logical.right) {
         bson_append_document_begin (&child, "1", 1, &child2);
         mongoc_matcher_op_to_bson (op->logical.right, &child2);
         bson_append_document_end (&child, &child2);
      }
      bson_append_array_end (bson, &child);
      break;
   case MONGOC_MATCHER_OPCODE_NOT:
      bson_append_document_begin (bson, op->not.path, -1, &child);
      bson_append_document_begin (&child, "$not", 4, &child2);
      mongoc_matcher_op_to_bson (op->not.child, &child2);
      bson_append_document_end (&child, &child2);
      bson_append_document_end (bson, &child);
      break;
   case MONGOC_MATCHER_OPCODE_EXISTS:
      BSON_APPEND_BOOL (bson, "$exists", op->exists.exists);
      break;
   case MONGOC_MATCHER_OPCODE_TYPE:
      BSON_APPEND_INT32 (bson, "$type", (int)op->type.type);
      break;
   default:
      BSON_ASSERT (FALSE);
      break;
   }
}

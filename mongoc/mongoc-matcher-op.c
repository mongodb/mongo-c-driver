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
   case MONGOC_MATCHER_OPCODE_NOT:
   case MONGOC_MATCHER_OPCODE_NOR:
      if (op->logical.left)
         mongoc_matcher_op_free (op->logical.left);
      if (op->logical.right)
         mongoc_matcher_op_free (op->logical.right);
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
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOT:
   case MONGOC_MATCHER_OPCODE_NOR:
      break;
   case MONGOC_MATCHER_OPCODE_EXISTS:
      return mongoc_matcher_op_exists_match (&op->exists, bson);
   case MONGOC_MATCHER_OPCODE_TYPE:
      return mongoc_matcher_op_type_match (&op->type, bson);
   default:
      break;
   }

   return FALSE;
}

#include <bson.h>
#include "mongoc-matcher-op-private.h"

typedef struct _mongoc_matcher {
   bson_t query;
   mongoc_matcher_op_t * optree;
} mongoc_matcher_t;

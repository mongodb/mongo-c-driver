#include <bson.h>

typedef struct _mongoc_matcher mongoc_matcher_t;

mongoc_matcher_t * mongoc_matcher_new(const bson_t * query);

bson_bool_t mongoc_matcher_match(const mongoc_matcher_t * matcher, const bson_t * query);

void  mongoc_matcher_destroy(mongoc_matcher_t * matcher);

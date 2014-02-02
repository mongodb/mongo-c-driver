typedef struct mongoc_matcher_op {
   bool (* execute)(struct mongoc_matcher_op *, const bson_t *);
   void (* pp)(struct mongoc_matcher_op *, bson_string_t *);
} mongoc_matcher_op_t;

typedef struct {
   mongoc_matcher_op_t base;
   enum OP_LOGIC;
   mongoc_matcher_op_t * left, *right;
} mongoc_matcher_op_logical_t;

typedef struct {
   mongoc_matcher_op_t base;
   enum OP_COMP;
   bson_iter_t * val;
   char * path;
} mongoc_matcher_op_comp_t;

typedef struct {
   mongoc_matcher_op_t base;
   char * path;
} mongoc_matcher_op_exists_t;

typedef struct {
   mongoc_matcher_op_t base;
   char * path;
   bson_type_t;
} mongoc_matcher_op_type_t;

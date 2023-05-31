#include <bson/bson.h>

#include <stdint.h>

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t len)
{
   bson_t *b = bson_new_from_json (data, (ssize_t) len, NULL);
   bson_destroy (b);
   return 0;
}

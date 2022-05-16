#include <bson/bson.h>

#include <stdint.h>

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t len)
{
   bson_t *b = bson_new_from_data (data, len);
   if (!b) {
      return 0;
   }
   bson_validate (b, 0xffffff, NULL);
   bson_destroy (b);
   return 0;
}

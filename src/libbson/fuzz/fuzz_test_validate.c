#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <bson/bson.h>

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
   bson_t b;
   if (bson_init_static (&b, data, size)) {
      bson_validate (&b,
                     BSON_VALIDATE_UTF8 | BSON_VALIDATE_DOLLAR_KEYS | BSON_VALIDATE_DOT_KEYS |
                        BSON_VALIDATE_UTF8_ALLOW_NULL | BSON_VALIDATE_EMPTY_KEYS,
                     NULL);
      return 0;
   }
   return -1;
}

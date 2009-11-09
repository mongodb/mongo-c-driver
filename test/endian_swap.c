/* endian_swap.c */

#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__
#define __BIG_ENDIAN__was_undef
#endif

#include "platform_hacks.h"

#ifndef __BIG_ENDIAN__was_undef
#undef __BIG_ENDIAN__
#undef __BIG_ENDIAN__was_undef
#endif

#include <stdio.h>

#define ASSERT(x) \
    do{ \
        if(!(x)){ \
            printf("failed assert: %s\n", #x); \
            return 1; \
        }\
    }while(0)


int main(){
    int small = 0x00112233;
    int64_t big = 0x0011223344556677;
    double d = 1.2345;

    ASSERT(bson_endian32(small) == 0x33221100);
    ASSERT(bson_endian32(bson_endian32(small)) == 0x00112233);

    ASSERT(bson_endian64(big) == 0x7766554433221100);
    ASSERT(bson_endian64(bson_endian64(big)) == 0x0011223344556677);

    ASSERT(bson_endian_to_double(bson_endian_from_double(d)) == 1.2345);

    return 0;
}

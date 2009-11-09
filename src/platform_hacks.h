/* platform_hacks.h */

/* all platform-specific ifdefs should go here */

#ifndef _PLATFORM_HACKS_H_
#define _PLATFORM_HACKS_H_

#ifdef __GNUC__
#define MONGO_INLINE static __inline__
#else
#define MONGO_INLINE static
#endif


#ifdef _MSC_VER
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

#ifdef __BIG_ENDIAN__
#define bson_endian_from_double(x) (bson_swap_endian64(bson_double_as_int64(x)))
#define bson_endian_to_double(x) (bson_int64_as_double(bson_swap_endian64(x)))
#define bson_endian64(x) (bson_swap_endian64(x))
#define bson_endian32(x) (bson_swap_endian32(x))
#else
#define bson_endian_from_double(x) (x)
#define bson_endian_to_double(x) (x)
#define bson_endian64(x) (x)
#define bson_endian32(x) (x)
#endif

/* don't call any of these directly */
MONGO_INLINE int64_t bson_double_as_int64(double x){
    return *(int64_t*)&x;
}
MONGO_INLINE double bson_int64_as_double(int64_t x){
    return *(double*)&x;
}
MONGO_INLINE int64_t bson_swap_endian64(int64_t x){
    union {
        int64_t i;
        char bytes[8];
    } in, out;

    in.i = x;

    out.bytes[0] = in.bytes[7];
    out.bytes[1] = in.bytes[6];
    out.bytes[2] = in.bytes[5];
    out.bytes[3] = in.bytes[4];
    out.bytes[4] = in.bytes[3];
    out.bytes[5] = in.bytes[2];
    out.bytes[6] = in.bytes[1];
    out.bytes[7] = in.bytes[0];

    return out.i;
}
MONGO_INLINE int bson_swap_endian32(int x){
    union {
        int i;
        char bytes[8];
    } in, out;

    in.i = x;

    out.bytes[0] = in.bytes[3];
    out.bytes[1] = in.bytes[2];
    out.bytes[2] = in.bytes[1];
    out.bytes[3] = in.bytes[0];

    return out.i;
}

#endif

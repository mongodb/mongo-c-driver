/* platform_hacks.h */

/* all platform-specific ifdefs should go here */

#ifndef _PLATFORM_HACKS_H_
#define _PLATFORM_HACKS_H_

#ifdef __GNUC__
#define MONGO_INLINE static __inline__
#else
#define MONGO_INLINE static
#endif


#if defined(MONGO_HAVE_STDINT) || __STDC_VERSION__ >= 199901L
#include <stdint.h>
#elif defined(MONGO_HAVE_UNISTD)
#include <unistd.h>
#elif defined(MONGO_USE__INT64)
typedef __int64 int64_t;
#elif defined(MONGO_USE_LONG_LONG_INT)
typedef long long int int64_t;
#else
#error must have a 64bit int type
#endif

#if defined(MONGO_HAVE_BOOL)
typedef bool bson_bool_t;
#elif defined(MONGO_HAVE_STDBOOL) || __STDC_VERSION__ >= 199901L
#include <stdbool.h>
typedef bool bson_bool_t;
#else
typedef unsigned char bson_bool_t;
#endif

/* big endian is only used for OID generation. little is used everywhere else */
#ifdef MONGO_BIG_ENDIAN
#define bson_little_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_little_endian32(out, in) ( bson_swap_endian32(out, in) )
#define bson_big_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_big_endian32(out, in) ( memcpy(out, in, 4) )
#else
#define bson_little_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_little_endian32(out, in) ( memcpy(out, in, 4) )
#define bson_big_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_big_endian32(out, in) ( bson_swap_endian32(out, in) )
#endif

MONGO_INLINE void bson_swap_endian64(void* outp, const void* inp){
    const char *in = inp;
    char *out = outp;

    out[0] = in[7];
    out[1] = in[6];
    out[2] = in[5];
    out[3] = in[4];
    out[4] = in[3];
    out[5] = in[2];
    out[6] = in[1];
    out[7] = in[0];

}
MONGO_INLINE void bson_swap_endian32(void* outp, const void* inp){
    const char *in = inp;
    char *out = outp;

    out[0] = in[3];
    out[1] = in[2];
    out[2] = in[1];
    out[3] = in[0];
}
#endif

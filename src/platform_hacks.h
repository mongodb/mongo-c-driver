/* platform_hacks.h */

/* all platform-specific ifdefs should go here */

#ifndef _PLATFORM_HACKS_H_
#define _PLATFORM_HACKS_H_

#ifdef _MSC_VER
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

#endif

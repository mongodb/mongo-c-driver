#include <stdlib.h>

#define ASSERT(x) \
    do{ \
        if(!(x)){ \
            printf("failed assert (%d): %s\n", __LINE__,  #x); \
            exit(1); \
        }\
    }while(0)

#ifdef _WIN32
#define INIT_SOCKETS_FOR_WINDOWS mongo_init_sockets();
#else
#define INIT_SOCKETS_FOR_WINDOWS do {} while(0)
#endif

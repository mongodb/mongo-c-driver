#include "mongo.h"
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

MONGO_EXTERN_C_START

int mongo_get_server_version( char *version ) {
    mongo conn[1];
    bson cmd[1], out[1];
    bson_iterator it[1];
    const char *result;

    mongo_connect( conn, TEST_SERVER, 27017 );

    bson_init( cmd );
    bson_append_int( cmd, "buildinfo", 1 );
    bson_finish( cmd );

    if( mongo_run_command( conn, "admin", cmd, out ) == MONGO_ERROR ) {
        return -1;
    }

    bson_iterator_init( it, out );
    result = bson_iterator_string( it );

    memcpy( version, result, strlen( result ) );

    return 0;
}

MONGO_EXTERN_C_END

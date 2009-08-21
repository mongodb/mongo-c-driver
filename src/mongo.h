/* mongo.h */

#ifndef _MONGO_H_
#define _MONGO_H_

#include "bson.h"

struct mongo_connection_options {
    const char * host;
    int port;
};

struct mongo_connection {
    int sock;
    struct mongo_connection_options options;
};

/**
 * @param options can be null
 */
int mongo_connect( struct mongo_connection * conn , struct mongo_connection_options * options );
int mongo_disconnect( struct mongo_connection * conn );
int mongo_destory( struct mongo_connection * conn );


void mongo_exit_on_error( int ret );

#endif

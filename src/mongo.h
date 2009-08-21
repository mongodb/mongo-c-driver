/* mongo.h */

#ifndef _MONGO_H_
#define _MONGO_H_

#include "bson.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

struct mongo_connection_options {
    const char * host;
    int port;
};

struct mongo_connection {
    struct mongo_connection_options * options;
    struct sockaddr_in sa;
    socklen_t addressSize;
    int sock;
};

struct mongo_message {
    int len;
    int id;
    int responseTo;
    int op;
    char data;
};

enum mongo_operations {
    mongo_op_msg = 1000,    /* generic msg command followed by a string */
    mongo_op_update = 2001, /* update object */
    mongo_op_insert = 2002,
    mongo_op_query = 2004,
    mongo_op_get_more = 2005,
    mongo_op_delete = 2006,
    mongo_op_kill_cursors = 2007
};


/* ----------------------------
   CONNECTION STUFF
   ------------------------------ */

/**
 * @param options can be null
 */
int mongo_connect( struct mongo_connection * conn , struct mongo_connection_options * options );
int mongo_disconnect( struct mongo_connection * conn );
int mongo_destory( struct mongo_connection * conn );



/* ----------------------------
   CORE METHODS - insert update remove query getmore
   ------------------------------ */

int mongo_insert( struct mongo_connection * conn , const char * ns , struct bson * data );
int mongo_insert_batch( struct mongo_connection * conn , const char * ns , struct bson ** data , int num );


/* ----------------------------
   HIGHER LEVEL - indexes - command helpers eval
   ------------------------------ */

/* ----------------------------
   COMMANDS
   ------------------------------ */


/* ----------------------------
   UTILS
   ------------------------------ */

void mongo_exit_on_error( int ret );

#endif

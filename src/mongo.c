/* mongo.c */

#include "mongo.h"

#include <stdlib.h>
#include <stdio.h>


int mongo_connect( struct mongo_connection * conn , struct mongo_connection_options * options ){
    return -1;
}

void mongo_exit_on_error( int ret ){
    if ( ret == 0 )
        return;
    
    printf( "error: %d\n" , ret );
    exit(ret);
}

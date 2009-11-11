/* test.c */

#include "mongo.h"
#include <stdio.h>
#include <string.h>

int main(){
    mongo_connection conn;
    mongo_connection_options opts;
    bson_buffer bb;
    bson b;

    bson_buffer_init( & bb );
    bson_append_double( &bb , "a" , 17 );
    bson_init( &b , bson_buffer_finish( &bb ) , 1 );
    
    strncpy(opts.host, TEST_SERVER, 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    mongo_exit_on_error( mongo_connect( &conn , &opts ) );
    mongo_exit_on_error( mongo_insert( &conn , "test.cc" , &b ) );
    
    mongo_query( &conn , "test.cc" , &b , 0 , 0 , 0 , 0 );
    
    mongo_destory( &conn );
    return 0;
}

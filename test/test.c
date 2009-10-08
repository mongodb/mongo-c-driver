/* test.c */

#include "mongo.h"
#include <stdio.h>

int main(){
    struct mongo_connection conn;
    struct bson_buffer bb;
    struct bson b;

    bson_buffer_init( & bb );
    bson_append_double( &bb , "a" , 17 );
    bson_init( &b , bson_finish( &bb ) , 1 );
    
    mongo_exit_on_error( mongo_connect( &conn , 0 ) );
    mongo_exit_on_error( mongo_insert( &conn , "test.cc" , &b ) );
    
    mongo_query( &conn , "test.cc" , &b , 0 , 0 , 0 , 0 );
    
    mongo_destory( &conn );
    return 0;
}

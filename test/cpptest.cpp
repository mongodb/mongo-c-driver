#include "test.h"
#include "mongo.h"
#include <iostream>
#include <cstring>
#include <cstdio>

// this is just a simple test to make sure everything works when compiled with a c++ compiler

using namespace std;

int main(){
    mongo_connection conn[1];
    bson_buffer bb;
    bson b;

    INIT_SOCKETS_FOR_WINDOWS;

    if (mongo_connect( conn, TEST_SERVER, 27017 )){
        cout << "failed to connect" << endl;
        return 1;
    }

    for(int i=0; i< 5; i++){
        bson_buffer_init( & bb );

        bson_append_new_oid( &bb, "_id" );
        bson_append_double( &bb , "a" , 17 );
        bson_append_int( &bb , "b" , 17 );
        bson_append_string( &bb , "c" , "17" );

        {
            bson_append_start_object(  &bb , "d" );
                bson_append_int( &bb, "i", 71 );
            bson_append_finish_object( &bb );
        }
        {
            bson_append_start_array(  &bb , "e" );
                bson_append_int( &bb, "0", 71 );
                bson_append_string( &bb, "1", "71" );
            bson_append_finish_object( &bb );
        }

        bson_from_buffer(&b, &bb);
        bson_destroy(&b);
    }

    mongo_destroy( conn );

    return 0;
}


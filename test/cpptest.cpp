#include "mongo.h"
#include <iostream>
#include <cstring>
#include <cstdio>

// this is just a simple test to make sure everything works when compiled with a c++ compiler
//
#define ASSERT(x) \
    do{ \
        if(!(x)){ \
            printf("failed assert (%d): %s\n", __LINE__,  #x); \
            return 1; \
        }\
    }while(0)

using namespace std;

int main(){
    mongo_connection conn[1];
    mongo_connection_options opts;
    bson_buffer bb;
    bson b;
    
    strncpy(opts.host, TEST_SERVER, 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    if (mongo_connect( conn , &opts )){
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
            bson_buffer * sub = bson_append_start_object(  &bb , "d" );
            bson_append_int( sub, "i", 71 );
            bson_append_finish_object(sub);
        }
        {
            bson_buffer * arr = bson_append_start_array(  &bb , "e" );
            bson_append_int( arr, "0", 71 );
            bson_append_string( arr, "1", "71" );
            bson_append_finish_object(arr);
        }

        bson_from_buffer(&b, &bb);
        bson_destroy(&b);
    }

    struct test_exception {};
    
    bool caught = false;
    try{
        MONGO_TRY{
            MONGO_THROW(MONGO_EXCEPT_NETWORK);
        }MONGO_CATCH{
            throw test_exception();
        }
    }catch (test_exception& e){
        caught = true;
    }

    ASSERT(caught);

    mongo_destroy( conn );

    return 0;
}


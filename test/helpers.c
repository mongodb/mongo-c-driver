/* helpers.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int test_index_helper( mongo *conn ) {

    bson_buffer bb;
    bson b, out;
    mongo_cursor c;
    bson_iterator it;

    bson_buffer_init( &bb );
    bson_append_int( &bb, "foo", 1 );
    bson_from_buffer( &b, &bb );

    mongo_create_index( conn, "test.bar", &b, MONGO_INDEX_SPARSE | MONGO_INDEX_UNIQUE, &out );

    bson_destroy( &b );

    bson_buffer_init( &bb );
    bson_append_start_object( &bb, "key" );
        bson_append_int( &bb, "foo", 1 );
    bson_append_finish_object( &bb );

    bson_from_buffer( &b, &bb );

    mongo_find_one( conn, "test.system.indexes", &b, NULL, &out );

    bson_print( &out );

    bson_iterator_init( &it, &out );

    ASSERT( bson_find( &it, &out, "unique" ) );
    ASSERT( bson_find( &it, &out, "sparse" ) );
}

int main() {

    mongo conn[1];

    if( mongo_connect( conn, TEST_SERVER, 27017 ) != MONGO_OK ) {
        printf("Failed to connect");
        exit(1);
    }


    test_index_helper( conn );

    return 0;
}

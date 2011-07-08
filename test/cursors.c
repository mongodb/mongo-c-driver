/* cursors.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int insert_sample_data( mongo_connection *conn ) {
    bson_buffer bb;
    bson b;
    int i;

    /* Insert 100,000 simple documents. */
    for( i=0; i<100000; i++ ) {
        bson_buffer_init( &bb );
        bson_append_int( &bb, "a", i );
        bson_from_buffer( &b, &bb );

        mongo_insert( conn, "test.cursors", &b );

        bson_destroy( &b );
    }
}

int remove_sample_data( mongo_connection *conn ) {
    mongo_cmd_drop_collection( conn, "test", "cursors", NULL );
}

int test_multiple_getmore( mongo_connection *conn ) {
    mongo_cursor *cursor;
    bson b;
    int count;

    insert_sample_data( conn );

    cursor = mongo_find( conn, "test.cursors", bson_empty( &b ), bson_empty( &b ), 0, 0, 0 );

    count = 0;
    while( mongo_cursor_next( cursor ) == MONGO_OK )
        count++;

    printf( "%d\n", count );
    ASSERT( count == 100000 );

    ASSERT( mongo_cursor_next( cursor ) == MONGO_ERROR );
    printf( cursor

    mongo_cursor_destroy( cursor );
    remove_sample_data( conn );
    return 0;
}

int test_tailable( mongo_connection *conn ) {
    insert_sample_data( conn );

    remove_sample_data( conn );
    return 0;
}

int main() {

    mongo_connection conn[1];
    bson_buffer bb;
    bson b;
    int res;
    time_t t1, t2;

    if( mongo_connect( conn, TEST_SERVER, 27017 ) != MONGO_OK ) {
        printf("Failed to connect");
        exit(1);
    }

    test_multiple_getmore( conn );

    return 0;
}

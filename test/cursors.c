/* cursors.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int create_capped_collection( mongo_connection *conn ) {
    bson_buffer bb;
    bson b;

    bson_buffer_init( &bb );
    bson_append_string( &bb, "create", "cursors" );
    bson_append_bool( &bb, "capped", 1 );
    bson_append_int( &bb, "size", 1000000 );
    bson_from_buffer( &b, &bb );

    ASSERT( mongo_run_command( conn, "test", &b, NULL ) == MONGO_OK );
}

int insert_sample_data( mongo_connection *conn, int n ) {
    bson_buffer bb;
    bson b;
    int i;

    create_capped_collection( conn );

    for( i=0; i<n; i++ ) {
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

    insert_sample_data( conn, 10000 );

    cursor = mongo_find( conn, "test.cursors", bson_empty( &b ), bson_empty( &b ), 0, 0, 0 );

    count = 0;
    while( mongo_cursor_next( cursor ) == MONGO_OK )
        count++;

    ASSERT( count == 10000 );

    ASSERT( mongo_cursor_next( cursor ) == MONGO_ERROR );
    ASSERT( cursor->err == MONGO_CURSOR_EXHAUSTED );

    mongo_cursor_destroy( cursor );
    remove_sample_data( conn );
    return 0;
}

int test_tailable( mongo_connection *conn ) {
    mongo_cursor *cursor;
    bson_buffer bb;
    bson b, q;
    int count;

    insert_sample_data( conn, 10000 );

    bson_buffer_init( &bb );
    bson_append_start_object( &bb, "$query" );
    bson_append_finish_object( &bb );
    bson_append_start_object( &bb, "$sort" );
    bson_append_int( &bb, "$natural", -1 );
    bson_append_finish_object( &bb );
    bson_from_buffer( &q, &bb );

    cursor = mongo_find( conn, "test.cursors", &q, bson_empty( &b ), 0, 0, MONGO_TAILABLE );

    count = 0;
    while( mongo_cursor_next( cursor ) == MONGO_OK )
        count++;

    ASSERT( count == 10000 );

    ASSERT( mongo_cursor_next( cursor ) == MONGO_ERROR );
    ASSERT( cursor->err == MONGO_CURSOR_PENDING );

    insert_sample_data( conn, 10 );

    count = 0;
    while( mongo_cursor_next( cursor ) == MONGO_OK ) {
        count++;
    }

    ASSERT( count == 10 );

    ASSERT( mongo_cursor_next( cursor ) == MONGO_ERROR );
    ASSERT( cursor->err == MONGO_CURSOR_PENDING );

    mongo_cursor_destroy( cursor );
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

    remove_sample_data( conn );
    test_multiple_getmore( conn );
    test_tailable( conn );

    return 0;
}

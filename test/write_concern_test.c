/* write_concern_test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/* TODO remove and add mongo_create_collection to the public API. */
void create_capped_collection( mongo *conn ) {
    bson b;

    mongo_cmd_drop_collection( conn, "test", "wc", NULL );

    bson_init( &b );
    bson_append_string( &b, "create", "wc" );
    bson_append_bool( &b, "capped", 1 );
    bson_append_int( &b, "size", 1000000 );
    bson_finish( &b );

    ASSERT( mongo_run_command( conn, "test", &b, NULL ) == MONGO_OK );

    bson_destroy( &b );
}

/* We can test write concern for update
 * and remove by doing operations on a capped collection. */
void test_update_and_remove( mongo *conn ) {
    mongo_write_concern wc[1];
    bson *objs[5];
    bson query[1], update[1];
    bson empty;
    int i;

    create_capped_collection( conn );

    for( i=0; i<5; i++ ) {
        objs[i] = bson_malloc( sizeof( bson ) );
        bson_init( objs[i] );
        bson_append_int( objs[i], "n", i );
        bson_finish( objs[i] );
    }

    ASSERT( mongo_insert_batch( conn, "test.wc", (const bson **)objs, 5,
        NULL ) == MONGO_OK );

    ASSERT( mongo_count( conn, "test", "wc", bson_empty( &empty ) ) == 5 );

    bson_init( query );
    bson_append_int( query, "n", 2 );
    bson_finish( query );

    ASSERT( mongo_find_one( conn, "test.wc", query, bson_empty( &empty ), NULL ) == MONGO_OK );

    bson_init( update );
        bson_append_start_object( update, "$set" );
            bson_append_string( update, "n", "a big long string" );
        bson_append_finish_object( update );
    bson_finish( update );

    /* Update will appear to succeed with no write concern specified, but doesn't. */
    ASSERT( mongo_find_one( conn, "test.wc", query, bson_empty( &empty ), NULL ) == MONGO_OK );
    ASSERT( mongo_update( conn, "test.wc", query, update, 0, NULL ) == MONGO_OK );
    ASSERT( mongo_find_one( conn, "test.wc", query, bson_empty( &empty ), NULL ) == MONGO_OK );

    /* Remove will appear to succeed with no write concern specified, but doesn't. */
    ASSERT( mongo_remove( conn, "test.wc", query, NULL ) == MONGO_OK );
    ASSERT( mongo_find_one( conn, "test.wc", query, bson_empty( &empty ), NULL ) == MONGO_OK );

    mongo_write_concern_init( wc );
    wc->w = 1;
    mongo_write_concern_finish( wc );

    mongo_clear_errors( conn );
    ASSERT( mongo_update( conn, "test.wc", query, update, 0, wc ) == MONGO_ERROR );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "failing update: objects in a capped ns cannot grow" );

    mongo_clear_errors( conn );
    ASSERT( mongo_remove( conn, "test.wc", query, wc ) == MONGO_ERROR );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "can't remove from a capped collection" );

    mongo_write_concern_destroy( wc );
    bson_destroy( query );
    bson_destroy( update );
    for( i=0; i<5; i++ ) {
        bson_destroy( objs[i] );
        bson_free( objs[i] );
    }
}

void test_write_concern_input( mongo *conn ) {
    mongo_write_concern wc[1], wcbad[1];
    bson b[1];

    mongo_cmd_drop_collection( conn, TEST_DB, TEST_COL, NULL );

    bson_init( b );
    bson_append_new_oid( b, "_id" );
    bson_finish( b );

    mongo_write_concern_init( wc );
    wc->w = 1;

    /* Failure to finish write concern object. */
    ASSERT( mongo_insert( conn, TEST_NS, b, wc ) != MONGO_OK );
    ASSERT( conn->err == MONGO_WRITE_CONCERN_INVALID );
    ASSERT_EQUAL_STRINGS( conn->errstr,
        "Must call mongo_write_concern_finish() before using *write_concern." );

    mongo_write_concern_finish( wc );

    /* Use a bad write concern. */
    mongo_clear_errors( conn );
    mongo_write_concern_init( wcbad );
    wcbad->w = 2;
    mongo_write_concern_finish( wcbad );
    mongo_set_write_concern( conn, wcbad );
    ASSERT( mongo_insert( conn, TEST_NS, b, NULL ) != MONGO_OK );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "norepl" );

    /* Ensure that supplied write concern overrides default. */
    mongo_clear_errors( conn );
    ASSERT( mongo_insert( conn, TEST_NS, b, wc ) != MONGO_OK );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->errstr, "See conn->lasterrstr for details." );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "E11000 duplicate key error index" );
    ASSERT( conn->lasterrcode == 11000 );

    conn->write_concern = NULL;
    mongo_write_concern_destroy( wc );
    mongo_write_concern_destroy( wcbad );
}

void test_insert( mongo *conn ) {
    mongo_write_concern wc[1];
    bson b[1], b2[1], b3[1], empty[1];
    bson *objs[2];

    mongo_cmd_drop_collection( conn, TEST_DB, TEST_COL, NULL );

    bson_init( b );
    bson_append_new_oid( b, "_id" );
    bson_finish( b );

    ASSERT( mongo_insert( conn, TEST_NS, b, NULL ) == MONGO_OK );

    /* This fails but returns OK because it doesn't use a write concern. */
    ASSERT( mongo_insert( conn, TEST_NS, b, NULL ) == MONGO_OK );

    mongo_write_concern_init( wc );
    wc->w = 1;
    mongo_write_concern_finish( wc );

    ASSERT( mongo_insert( conn, TEST_NS, b, wc ) == MONGO_ERROR );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->errstr, "See conn->lasterrstr for details." );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "E11000 duplicate key error index" );
    ASSERT( conn->lasterrcode == 11000 );
    mongo_clear_errors( conn );

    /* Still fails but returns OK because it doesn't use a write concern. */
    ASSERT( mongo_insert( conn, TEST_NS, b, NULL ) == MONGO_OK );

    /* But not when we set a default write concern on the conn. */
    mongo_set_write_concern( conn, wc );
    ASSERT( mongo_insert( conn, TEST_NS, b, NULL ) != MONGO_OK );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->errstr, "See conn->lasterrstr for details." );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "E11000 duplicate key error index" );
    ASSERT( conn->lasterrcode == 11000 );

    /* Now test batch insert. */
    bson_init( b2 );
    bson_append_new_oid( b2, "_id" );
    bson_finish( b2 );

    bson_init( b3 );
    bson_append_new_oid( b3, "_id" );
    bson_finish( b3 );

    objs[0] = b2;
    objs[1] = b3;

    /* Insert two new documents by insert_batch. */
    conn->write_concern = NULL;
    ASSERT( mongo_count( conn, TEST_DB, TEST_COL, bson_empty( empty ) ) == 1 );
    ASSERT( mongo_insert_batch( conn, TEST_NS, (const bson **)objs, 2, NULL ) == MONGO_OK );
    ASSERT( mongo_count( conn, TEST_DB, TEST_COL, bson_empty( empty ) ) == 3 );

    /* This should definitely fail if we try again with write concern. */
    mongo_clear_errors( conn );
    ASSERT( mongo_insert_batch( conn, TEST_NS, (const bson **)objs, 2, wc ) == MONGO_ERROR );
    ASSERT( conn->err == MONGO_WRITE_ERROR );
    ASSERT_EQUAL_STRINGS( conn->errstr, "See conn->lasterrstr for details." );
    ASSERT_EQUAL_STRINGS( conn->lasterrstr, "E11000 duplicate key error index" );
    ASSERT( conn->lasterrcode == 11000 );

    /* But it will succeed without the write concern set. */
    ASSERT( mongo_insert_batch( conn, TEST_NS, (const bson **)objs, 2, NULL ) == MONGO_OK );

    bson_destroy( b );
    bson_destroy( b2 );
    bson_destroy( b3 );
    mongo_write_concern_destroy( wc );
}

int main() {
    mongo conn[1];
    char version[10];

    INIT_SOCKETS_FOR_WINDOWS;

    if( mongo_connect( conn, TEST_SERVER, 27017 ) != MONGO_OK ) {
        printf( "failed to connect\n" );
        exit( 1 );
    }

    test_insert( conn );
    if( mongo_get_server_version( version ) != -1 && version[0] != '1' ) {
        test_write_concern_input( conn );
        test_update_and_remove( conn );
    }

    mongo_destroy( conn );
    return 0;
}

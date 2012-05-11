/* write_concern_test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void test_bad_input( mongo *conn ) {
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
    bson b[1];

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

    bson_destroy( b );
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
        test_bad_input( conn );
    }

    mongo_destroy( conn );
    return 0;
}

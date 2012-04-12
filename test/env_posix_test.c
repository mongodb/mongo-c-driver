/* env_posix_test.c
 * Test posix-specific features.
 */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test read timeout by causing the
 * server to sleep for 10s on a query.
 */
int test_read_timeout( void ) {
    mongo conn[1];
    bson b, obj, out, fields;
    int i;
    int res;

    const char *db = "test";
    const char *col = "c.simple";
    const char *ns = "test.c.simple";

    if ( mongo_connect( conn, TEST_SERVER, 27017 ) ) {
        printf( "failed to connect\n" );
        exit( 1 );
    }

    bson_init( &b );
    bson_append_code( &b, "$where", "sleep( 10 * 1000 );");
    bson_finish( &b );

    bson_init( &obj );
    bson_append_string( &obj, "foo", "bar");
    bson_finish( &obj );

    res = mongo_insert( conn, "test.foo", &obj );

    /* Set the connection timeout here. */
    mongo_set_op_timeout( conn, 1000 );

    res = mongo_find_one( conn, "test.foo", &b, bson_empty(&fields), &out );
    ASSERT( res == MONGO_ERROR );

    ASSERT( conn->err == MONGO_IO_ERROR );
    ASSERT( strcmp( "Resource temporarily unavailable", conn->errstr ) == 0 );

    return 0;
}

/* Test getaddrinfo() by successfully connecting to 'localhost'. */
int test_getaddrinfo( void ) {
    mongo conn[1];
    bson b[1];
    char *ns = "test.foo";

    if( mongo_connect( conn, "localhost", 27017 ) != MONGO_OK ) {
        printf( "failed to connect\n" );
        exit( 1 );
    }

    mongo_cmd_drop_collection( conn, "test", "foo", NULL );

    bson_init( b );
    bson_append_int( b, "foo", 17 );
    bson_finish( b );

    mongo_insert( conn , ns , b );

    ASSERT( mongo_count( conn, "test", "foo", NULL ) == 1 );

    bson_destroy( b );
    mongo_destroy( conn );


    return 0;
}

int main() {
    test_read_timeout();
    test_getaddrinfo();

    return 0;
}

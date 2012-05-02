#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *db = "test";
static const char *ns = "test.c.error";

int test_insert_limits( void ) {
    char version[10];
    mongo conn[1];
    int max_server_bson_size = 0;
    int i;
    char key[10];
    bson b[1], b2[1];
    bson *objs[2];
   
    /* Test the default max BSON size. */
    mongo_init( conn );
    ASSERT( conn->max_bson_size == MONGO_DEFAULT_MAX_BSON_SIZE );

    /* We'll perform the full test if we're running v2.0 or later. */
    if( mongo_get_server_version( version ) != -1 && version[0] <= '1' )
        return 0;

    if ( mongo_connect( conn , TEST_SERVER, 27017 ) ) {
        printf( "failed to connect\n" );
        exit( 1 );
    }

    ASSERT( conn->max_bson_size > MONGO_DEFAULT_MAX_BSON_SIZE );

    bson_init( b );
    for(i=0; i<1200000; i++) {
        sprintf( key, "%d", i + 10000000 );
        bson_append_int( b, key, i );
    }
    bson_finish( b );

    ASSERT( bson_size( b ) > conn->max_bson_size );

    ASSERT( mongo_insert( conn, "test.foo", b ) == MONGO_ERROR );
    ASSERT( conn->err == MONGO_BSON_TOO_LARGE );
    
    mongo_clear_stored_errors( conn );
    ASSERT( conn->err == 0 );

    bson_init( b2 );
    bson_append_int( b2, "foo", 1 );
    bson_finish( b2 );

    objs[0] = b;
    objs[1] = b2;

    ASSERT( mongo_insert_batch( conn, "test.foo", objs, 2 ) == MONGO_ERROR );
    ASSERT( conn->err == MONGO_BSON_TOO_LARGE );

    return 0;
}

int main() {
    mongo conn[1];
    bson obj;

    INIT_SOCKETS_FOR_WINDOWS;

    if ( mongo_connect( conn , TEST_SERVER, 27017 ) ) {
        printf( "failed to connect\n" );
        exit( 1 );
    }

    /*********************/
    ASSERT( mongo_cmd_get_prev_error( conn, db, NULL ) == MONGO_OK );
    ASSERT( conn->lasterrcode == 0 );
    ASSERT( conn->lasterrstr[0] == 0 );

    ASSERT( mongo_cmd_get_last_error( conn, db, NULL ) == MONGO_OK );
    ASSERT( conn->lasterrcode == 0 );
    ASSERT( conn->lasterrstr[0] == 0 );

    ASSERT( mongo_cmd_get_prev_error( conn, db, &obj ) == MONGO_OK );
    bson_destroy( &obj );

    ASSERT( mongo_cmd_get_last_error( conn, db, &obj ) == MONGO_OK );
    bson_destroy( &obj );

    /*********************/
    mongo_simple_int_command( conn, db, "forceerror", 1, NULL );

    ASSERT( mongo_cmd_get_prev_error( conn, db, NULL ) == MONGO_ERROR );
    ASSERT( conn->lasterrcode == 10038 );
    ASSERT( strcmp( ( const char * )conn->lasterrstr, "forced error" ) == 0 );

    ASSERT( mongo_cmd_get_last_error( conn, db, NULL ) == MONGO_ERROR );

    ASSERT( mongo_cmd_get_prev_error( conn, db, &obj ) == MONGO_ERROR );
    bson_destroy( &obj );

    ASSERT( mongo_cmd_get_last_error( conn, db, &obj ) == MONGO_ERROR );
    bson_destroy( &obj );

    /* should clear lasterror but not preverror */
    mongo_find_one( conn, ns, bson_empty( &obj ), bson_empty( &obj ), NULL );

    ASSERT( mongo_cmd_get_prev_error( conn, db, NULL ) == MONGO_ERROR );
    ASSERT( mongo_cmd_get_last_error( conn, db, NULL ) == MONGO_OK );

    ASSERT( mongo_cmd_get_prev_error( conn, db, &obj ) == MONGO_ERROR );
    bson_destroy( &obj );

    ASSERT( mongo_cmd_get_last_error( conn, db, &obj ) == MONGO_OK );
    bson_destroy( &obj );

    /*********************/
    mongo_cmd_reset_error( conn, db );

    ASSERT( mongo_cmd_get_prev_error( conn, db, NULL ) == MONGO_OK );
    ASSERT( mongo_cmd_get_last_error( conn, db, NULL ) == MONGO_OK );

    ASSERT( mongo_cmd_get_prev_error( conn, db, &obj ) == MONGO_OK );
    bson_destroy( &obj );

    ASSERT( mongo_cmd_get_last_error( conn, db, &obj ) == MONGO_OK );
    bson_destroy( &obj );


    mongo_cmd_drop_db( conn, db );
    mongo_destroy( conn );

    test_insert_limits();

    return 0;
}

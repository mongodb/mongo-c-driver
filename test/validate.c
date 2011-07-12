/* validate.c */

#include "test.h"
#include "mongo.h"
#include "encoding.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BATCH_SIZE 10

static void make_small_invalid( bson * out, int i ) {
    bson_buffer bb;
    bson_buffer_init(&bb);
    bson_append_new_oid(&bb, "$_id");
    bson_append_int(&bb, "x.foo", i);
    bson_from_buffer(out, &bb);
}

int main() {
    mongo *conn = mongo_new();
    bson_buffer bb;
    bson b, empty;
    unsigned char not_utf8[3];
    int result = 0;
    const char * ns = "test.c.validate";

    int i, j;
    bson bs[BATCH_SIZE];
    bson *bp[BATCH_SIZE];

    not_utf8[0] = 0xC0;
    not_utf8[1] = 0xC0;
    not_utf8[2] = '\0';

    INIT_SOCKETS_FOR_WINDOWS;

    if (mongo_connect( conn , TEST_SERVER, 27017 )){
        printf("failed to connect\n");
        exit(1);
    }

    /* Test valid keys. */
    bson_buffer_init( & bb );
    result = bson_append_string( &bb , "a.b" , "17" );
    ASSERT( result == BSON_OK );

    ASSERT( bb.err & BSON_FIELD_HAS_DOT );

    result = bson_append_string( &bb , "$ab" , "17" );
    ASSERT( result == BSON_OK );
    ASSERT( bb.err & BSON_FIELD_INIT_DOLLAR );

    result = bson_append_string( &bb , "ab" , "this is valid utf8" );
    ASSERT( result == BSON_OK );
    ASSERT( ! (bb.err & BSON_NOT_UTF8 ) );

    result = bson_append_string( &bb , (const char*)not_utf8, "valid" );
    ASSERT( result == BSON_ERROR );
    ASSERT( bb.err & BSON_NOT_UTF8 );

    bson_from_buffer(&b, &bb);
    ASSERT( b.err & BSON_FIELD_HAS_DOT );
    ASSERT( b.err & BSON_FIELD_INIT_DOLLAR );
    ASSERT( b.err & BSON_NOT_UTF8 );

    result = mongo_insert( conn, ns, &b );
    ASSERT( result == MONGO_ERROR );
    ASSERT( conn->err == MONGO_BSON_INVALID );

    result = mongo_update( conn, ns, bson_empty( &empty ), &b, 0 );
    ASSERT( result == MONGO_ERROR );
    ASSERT( conn->err == MONGO_BSON_INVALID );

    bson_destroy(&b);

    /* Test valid strings. */
    bson_buffer_init( & bb );
    result = bson_append_string( &bb , "foo" , "bar" );
    ASSERT( result == BSON_OK );
    ASSERT( bb.err == 0 );

    result = bson_append_string( &bb , "foo" , (const char*)not_utf8 );
    ASSERT( result == BSON_ERROR );
    ASSERT( bb.err & BSON_NOT_UTF8 );

    bb.err = 0;
    ASSERT( bb.err == 0 );

    result = bson_append_regex( &bb , "foo" , (const char*)not_utf8, "s" );
    ASSERT( result == BSON_ERROR );
    ASSERT( bb.err & BSON_NOT_UTF8 );

    for (j=0; j < BATCH_SIZE; j++)
        bp[j] = &bs[j];

    for (j=0; j < BATCH_SIZE; j++)
        make_small_invalid(&bs[j], i);

    result = mongo_insert_batch( conn, ns, bp, BATCH_SIZE );
    ASSERT( result == MONGO_ERROR );
    ASSERT( conn->err == MONGO_BSON_INVALID );

    for (j=0; j < BATCH_SIZE; j++)
        bson_destroy(&bs[j]);

    mongo_cmd_drop_db(conn, "test");
    mongo_disconnect( conn );

    mongo_destroy( conn );

    return 0;
}

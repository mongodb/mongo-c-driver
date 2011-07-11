/* test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    mongo_connection conn[1];
    bson_buffer bb;
    bson b;
    mongo_cursor * cursor;
    int i;
    char hex_oid[25];
    bson_timestamp_t ts = { 1, 2 };

    const char * col = "c.simple";
    const char * ns = "test.c.simple";

    /* mongo_connect( conn, TEST_SERVER, 27017 ); */

    /* Simple connect API
    mongo *conn = mongo_new();
    mongo_connect( conn, TEST_SERVER, 27017 );
    mongo_destroy( conn );

    * Advanced and replica set API
    mongo *conn = mongo_new();
    mongo_set_connect_timeout( conn, 1000 );
    mongo_set_op_timeout( conn, 1000 )
    mongo_set_replset( conn, "foobar" );
    mongo_replset_connect( conn );
    mongo_destroy( conn );

    * BSON API
    bson *obj = bson_new();
    bson_append_int( obj, "a", 1 );
    mongo_insert( conn, obj );
    bson_destroy( obj );

    * Cursor API
    mongo_cursor *cursor = mongo_cursor_new( conn, "test.ns" );
    mongo_cursor_limit( cursor, 100 );
    mongo_cursor_skip( cursor, 100 );
    mongo_cursor_query( cursor, query );
    mongo_cursor_fields( cursor, fields );
    data = mongo_cursor_next( cursor );
    mongo_cursor_destroy( cursor );
    */

    INIT_SOCKETS_FOR_WINDOWS;

    if( mongo_connect( conn , TEST_SERVER, 27017 ) != MONGO_OK ){
        printf("failed to connect\n");
        exit(1);
    }

    mongo_cmd_drop_collection(conn, "test", col, NULL);
    mongo_find_one(conn, ns, bson_empty(&b), bson_empty(&b), NULL);

    for(i=0; i< 5; i++){
        bson_buffer_init( & bb );

        bson_append_new_oid( &bb, "_id" );
        bson_append_timestamp( &bb, "ts", &ts );
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
        mongo_insert( conn , ns , &b );
        bson_destroy(&b);
    }

    cursor = mongo_find( conn , ns , bson_empty(&b) , 0 , 0 , 0 , 0 );

    while( mongo_cursor_next(cursor) == MONGO_OK ){
        bson_iterator it;
        bson_iterator_init(&it, cursor->current.data);
        while(bson_iterator_next(&it)){
            fprintf(stderr, "  %s: ", bson_iterator_key(&it));

            switch(bson_iterator_type(&it)){
                case BSON_DOUBLE:
                    fprintf(stderr, "(double) %e\n", bson_iterator_double(&it));
                    break;
                case BSON_INT:
                    fprintf(stderr, "(int) %d\n", bson_iterator_int(&it));
                    break;
                case BSON_STRING:
                    fprintf(stderr, "(string) \"%s\"\n", bson_iterator_string(&it));
                    break;
                case BSON_OID:
                    bson_oid_to_string(bson_iterator_oid(&it), hex_oid);
                    fprintf(stderr, "(oid) \"%s\"\n", hex_oid);
                    break;
                case BSON_OBJECT:
                    fprintf(stderr, "(subobject) {...}\n");
                    break;
                case BSON_ARRAY:
                    fprintf(stderr, "(array) [...]\n");
                    break;
                case BSON_TIMESTAMP:
                    fprintf(stderr, "(timestamp) [...]\n");
                    break;
                default:
                    fprintf(stderr, "(type %d)\n", bson_iterator_type(&it));
                    break;
            }
        }
        fprintf(stderr, "\n");
    }

    mongo_cursor_destroy(cursor);
    ASSERT( mongo_cmd_drop_db(conn, "test") == MONGO_OK );
    mongo_disconnect( conn );

    mongo_reconnect( conn );

    ASSERT( mongo_simple_int_command( conn, "admin", "ping", 1, NULL ) == MONGO_OK );

    mongo_destroy( conn );
    return 0;
}

/* test.c */

#include "mongo.h"
#include <stdio.h>
#include <string.h>

int main(){
    mongo_connection conn;
    mongo_connection_options opts;
    bson_buffer bb;
    bson b;
    mongo_cursor * cursor;
    int i;
    char hex_oid[25];

    
    strncpy(opts.host, TEST_SERVER, 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    mongo_exit_on_error( mongo_connect( &conn , &opts ) );

    /* TODO drop collection */
    for(i=0; i< 5; i++){
        bson_buffer_init( & bb );

        bson_append_new_oid( &bb, "_id" );
        bson_append_double(  &bb , "a" , 17 );
        bson_append_int(     &bb , "b" , 17 );
        bson_append_string(  &bb , "c" , "17" );

        bson_init( &b , bson_buffer_finish( &bb ) , 1 );
        mongo_insert( &conn , "test.cc" , &b );
        bson_destroy(&b);
    }
    
    cursor = mongo_query( &conn , "test.cc" , bson_empty(&b) , 0 , 0 , 0 , 0 );

    while (mongo_cursor_next(cursor)){
        bson_iterator it;
        bson_iterator_init(&it, cursor->current.data);
        while(bson_iterator_next(&it)){
            fprintf(stderr, "  %s: ", bson_iterator_key(&it));

            switch(bson_iterator_type(&it)){
                case bson_double:
                    fprintf(stderr, "(double) %e\n", bson_iterator_double(&it));
                    break;
                case bson_int:
                    fprintf(stderr, "(int) %d\n", bson_iterator_int(&it));
                    break;
                case bson_string:
                    fprintf(stderr, "(string) \"%s\"\n", bson_iterator_string(&it));
                    break;
                case bson_oid:
                    bson_oid_to_string(bson_iterator_oid(&it), hex_oid);
                    fprintf(stderr, "(oid) \"%s\"\n", hex_oid);
                    break;
                default:
                    fprintf(stderr, "(type %d)\n", bson_iterator_type(&it));
                    break;
            }
        }
        fprintf(stderr, "\n");
    }

    mongo_cursor_destroy(cursor);
    mongo_destory( &conn );
    return 0;
}

/* test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    mongo_connection conn[1];
    mongo_connection_options opts;
    bson_buffer bb;
    bson b;
    mongo_cursor * cursor;
    int i;
    char hex_oid[25];

    const char * col = "c.simple";
    const char * ns = "test.c.simple";

    INIT_SOCKETS_FOR_WINDOWS;
    
    strncpy(opts.host, TEST_SERVER, 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    if (mongo_connect( conn , &opts )){
        printf("failed to connect\n");
        exit(1);
    }

    /* if the collection doesn't exist dropping it will fail */
    if (!mongo_cmd_drop_collection(conn, "test", col, NULL)
          && mongo_find_one(conn, ns, bson_empty(&b), bson_empty(&b), NULL)){
        printf("failed to drop collection\n");
        exit(1);
    }

    for(i=0; i< 5; i++){
        bson_buffer_init( & bb );

        bson_append_new_oid( &bb, "_id" );
        bson_append_double( &bb , "a" , 17 );
        bson_append_int( &bb , "b" , 17 );
        bson_append_string( &bb , "c" , "17" );

        {
            bson_buffer * sub = bson_append_start_object(  &bb , "d" );
            bson_append_int( sub, "i", 71 );
            bson_append_finish_object(sub);
        }
        {
            bson_buffer * arr = bson_append_start_array(  &bb , "e" );
            bson_append_int( arr, "0", 71 );
            bson_append_string( arr, "1", "71" );
            bson_append_finish_object(arr);
        }

        bson_from_buffer(&b, &bb);
        mongo_insert( conn , ns , &b );
        bson_destroy(&b);
    }
    
    cursor = mongo_find( conn , ns , bson_empty(&b) , 0 , 0 , 0 , 0 );

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
                case bson_object:
                    fprintf(stderr, "(subobject) {...}\n");
                    break;
                case bson_array:
                    fprintf(stderr, "(array) [...]\n");
                    break;
                default:
                    fprintf(stderr, "(type %d)\n", bson_iterator_type(&it));
                    break;
            }
        }
        fprintf(stderr, "\n");
    }

    mongo_cursor_destroy(cursor);
    mongo_destroy( conn );
    return 0;
}

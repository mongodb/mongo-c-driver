#include "test.h"
#include "bson.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    bson_buffer bb;
    bson b, sub;
    bson_iterator it;
    bson_type type;

    /* Create a rich document like this one:
     *
     * { _id: ObjectId("4d95ea712b752328eb2fc2cc"),
     *   user_id: ObjectId("4d95ea712b752328eb2fc2cd"),
     *
     *   items: [
     *     { sku: "col-123",
     *       name: "John Coltrane: Impressions",
     *       price: 1099,
     *     },
     *
     *     { sku: "young-456",
     *       name: "Larry Young: Unity",
     *       price: 1199
     *     }
     *   ],
     *
     *   address: {
     *     street: "59 18th St.",
     *     zip: 10010
     *   },
     *
     *   total: 2298
     * }
     */
    bson_buffer_init( &bb );
    bson_append_new_oid( &bb, "_id" );
    bson_append_new_oid( &bb, "user_id" );

    bson_append_start_array( &bb, "items" );
        bson_append_start_object( &bb, "0" );
            bson_append_string( &bb, "name", "John Coltrane: Impressions" );
            bson_append_int( &bb, "price", 1099 );
        bson_append_finish_object( &bb );

        bson_append_start_object( &bb, "1" );
            bson_append_string( &bb, "name", "Larry Young: Unity" );
            bson_append_int( &bb, "price", 1199 );
        bson_append_finish_object( &bb );
    bson_append_finish_object( &bb );

    bson_append_start_object( &bb, "address" );
        bson_append_string( &bb, "street", "59 18th St." );
        bson_append_int( &bb, "zip", 10010 );
    bson_append_finish_object( &bb );

    bson_append_int( &bb, "total", 2298 );

    /* Convert from a buffer to a raw BSON object that
     * can be sent to the server:
     */
    bson_from_buffer( &b, &bb );

    /* Advance to the 'items' array */
    bson_find( &it, &b, "items" );

    /* Get the subobject representing items */
    bson_iterator_subobject( &it, &sub );

    /* Now iterate that object */
    bson_print( &sub );

   return 0;
}

/*
void bson_iterate_object( bson* b, int depth ) {
    bson_iterator it;

    bson_iterator_init( &it, b->data );

    while ( bson_iterator_next( &it ) ){
        bson_type t = bson_iterator_type( &it );
        if ( t == 0 )
            break;
        key = bson_iterator_key( &i );

        for ( temp=0; temp<=depth; temp++ )
            printf( "\t" );
        printf( "%s : %d \t " , key , t );
        switch ( t ){
        case bson_int: printf( "%d" , bson_iterator_int( &i ) ); break;
        case bson_double: printf( "%f" , bson_iterator_double( &i ) ); break;
        case bson_bool: printf( "%s" , bson_iterator_bool( &i ) ? "true" : "false" ); break;
        case bson_string: printf( "%s" , bson_iterator_string( &i ) ); break;
        case bson_null: printf( "null" ); break;
        case bson_oid: bson_oid_to_string(bson_iterator_oid(&i), oidhex); printf( "%s" , oidhex ); break;
        case bson_timestamp:
            ts = bson_iterator_timestamp( &i );
            printf("i: %d, t: %d", ts.i, ts.t);
            break;
        case bson_object:
        case bson_array:
            printf( "\n" );
            bson_print_raw( bson_iterator_value( &i ) , depth + 1 );
            break;
        default:
            fprintf( stderr , "can't print type : %d\n" , t );
        }
        printf( "\n" );
    }
} */

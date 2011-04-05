#include "test.h"
#include "bson.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    bson_buffer bb;
    bson b, sub;
    bson_iterator it;

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

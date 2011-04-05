/* count_delete.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    mongo_connection conn[1];
    bson_buffer bb;
    bson b;
    int i;

    const char * db = "test";
    const char * col = "c.simple";
    const char * ns = "test.c.simple";

    INIT_SOCKETS_FOR_WINDOWS;

    if (mongo_connect( conn , TEST_SERVER , 27017 )){
        printf("failed to connect\n");
        exit(1);
    }

    /* if the collection doesn't exist dropping it will fail */
    if (!mongo_cmd_drop_collection(conn, "test", col, NULL)
          && mongo_count(conn, db, col, NULL) != 0){
        printf("failed to drop collection\n");
        exit(1);
    }

    for(i=0; i< 5; i++){
        bson_buffer_init( & bb );

        bson_append_new_oid( &bb, "_id" );
        bson_append_int( &bb , "a" , i+1 ); /* 1 to 5 */
        bson_from_buffer(&b, &bb);

        mongo_insert( conn , ns , &b );
        bson_destroy(&b);
    }

    /* query: {a: {$gt: 3}} */
    bson_buffer_init( & bb );
    {
        bson_buffer * sub = bson_append_start_object(&bb, "a");
        bson_append_int(sub, "$gt", 3);
        bson_append_finish_object(sub);
    }
    bson_from_buffer(&b, &bb);
    
    ASSERT(mongo_count(conn, db, col, NULL) == 5);
    ASSERT(mongo_count(conn, db, col, &b) == 2);

    mongo_remove(conn, ns, &b);

    ASSERT(mongo_count(conn, db, col, NULL) == 3);
    ASSERT(mongo_count(conn, db, col, &b) == 0);

    mongo_cmd_drop_db(conn, db);
    mongo_destroy(conn);
    return 0;
}

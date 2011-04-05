#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    mongo_connection conn[1];
    bson_buffer bb;
    bson obj;
    bson cond;
    int i;
    bson_oid_t oid;
    const char* col = "c.update_test";
    const char* ns = "test.c.update_test";

    INIT_SOCKETS_FOR_WINDOWS;

    if (mongo_connect( conn , TEST_SERVER, 27017 )){
        printf("failed to connect\n");
        exit(1);
    }

    /* if the collection doesn't exist dropping it will fail */
    if (!mongo_cmd_drop_collection(conn, "test", col, NULL)
          && mongo_find_one(conn, ns, bson_empty(&obj), bson_empty(&obj), NULL)){
        printf("failed to drop collection\n");
        exit(1);
    }

    bson_oid_gen(&oid);

    { /* insert */
        bson_buffer_init(&bb);
        bson_append_oid(&bb, "_id", &oid);
        bson_append_int(&bb, "a", 3 );
        bson_from_buffer(&obj, &bb);
        mongo_insert(conn, ns, &obj);
        bson_destroy(&obj);
    }

    { /* insert */
        bson op;

        bson_buffer_init(&bb);
        bson_append_oid(&bb, "_id", &oid);
        bson_from_buffer(&cond, &bb);

        bson_buffer_init(&bb);
        {
            bson_buffer * sub = bson_append_start_object(&bb, "$inc");
            bson_append_int(sub, "a", 2 );
            bson_append_finish_object(sub);
        }
        {
            bson_buffer * sub = bson_append_start_object(&bb, "$set");
            bson_append_double(sub, "b", -1.5 );
            bson_append_finish_object(sub);
        }
        bson_from_buffer(&op, &bb);

        for (i=0; i<5; i++)
            mongo_update(conn, ns, &cond, &op, 0);

        /* cond is used later */
        bson_destroy(&op);
    }
    
    if(!mongo_find_one(conn, ns, &cond, 0, &obj)){
        printf("Failed to find object\n");
        exit(1);
    } else {
        int fields = 0;
        bson_iterator it;
        bson_iterator_init(&it, obj.data);

        bson_destroy(&cond);

        while(bson_iterator_next(&it)){
            switch(bson_iterator_key(&it)[0]){
                case '_': /* id */
                    ASSERT(bson_iterator_type(&it) == bson_oid);
                    ASSERT(!memcmp(bson_iterator_oid(&it)->bytes, oid.bytes, 12));
                    fields++;
                    break;
                case 'a':
                    ASSERT(bson_iterator_type(&it) == bson_int);
                    ASSERT(bson_iterator_int(&it) == 3 + 5*2);
                    fields++;
                    break;
                case 'b':
                    ASSERT(bson_iterator_type(&it) == bson_double);
                    ASSERT(bson_iterator_double(&it) == -1.5);
                    fields++;
                    break;
            }
        }

        ASSERT(fields == 3);
    }

    bson_destroy(&obj);

    mongo_cmd_drop_db(conn, "test");
    mongo_destroy(conn);
    return 0;
}

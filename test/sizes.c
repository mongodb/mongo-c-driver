/* sizes.c */

#include "mongo.h"
#include <stdio.h>


#define ASSERT(x) \
    do{ \
        if(!(x)){ \
            printf("failed assert: %s\n", #x); \
            return 1; \
        }\
    }while(0)


int main(){
    mongo_reply mr;

    ASSERT(sizeof(int) == 4);
    ASSERT(sizeof(int64_t) == 8);
    ASSERT(sizeof(double) == 8);
    ASSERT(sizeof(bson_oid_t) == 12);

    ASSERT(sizeof(mongo_header) == 4+4+4+4);
    ASSERT(sizeof(mongo_reply_fields) == 4+8+4+4);

    /* field offset of obj in mongo_reply */
    ASSERT((&mr.objs - (char*)&mr) == (4+4+4+4 + 4+8+4+4));

    return 0;
}

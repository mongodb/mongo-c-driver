#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static mongo_connection conn[1];
static mongo_connection_options opts;
static const char* db = "test";

int main(){

    INIT_SOCKETS_FOR_WINDOWS;

    strncpy(opts.host, TEST_SERVER, 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    if (mongo_connect( conn , &opts )){
        printf("failed to connect\n");
        exit(1);
    }

    mongo_cmd_drop_db(conn, db);

    ASSERT(mongo_cmd_authenticate(conn, db, "user", "password") == 0);
    mongo_cmd_add_user(conn, db, "user", "password");
    ASSERT(mongo_cmd_authenticate(conn, db, "user", "password") == 1);

    return 0;
}

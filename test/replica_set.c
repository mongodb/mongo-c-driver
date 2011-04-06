/* test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int test_connect( const char* set_name ) {

    mongo_connection conn[1];
    int res;

    INIT_SOCKETS_FOR_WINDOWS;

    mongo_replset_init_conn( conn, set_name );
    mongo_replset_add_seed( conn, TEST_SERVER, 30000 );
    mongo_replset_add_seed( conn, TEST_SERVER, 30001 );

    if( res = mongo_replset_connect( conn ) ) {
        mongo_destroy( conn );
        return res;
    }
    else {
        mongo_disconnect( conn );
        return mongo_reconnect( conn );
    }
}

int main() {
    ASSERT( test_connect( "test-rs" ) == 0 );
    ASSERT( test_connect( "test-foobar" ) == mongo_conn_bad_set_name );

    return 0;
}

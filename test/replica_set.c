/* test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

int test_reconnect( const char* set_name ) {

    mongo_connection conn[1];
    int res;
    int e = 0;
    bson b;

    INIT_SOCKETS_FOR_WINDOWS;

    mongo_replset_init_conn( conn, set_name );
    mongo_replset_add_seed( conn, TEST_SERVER, 30000 );
    mongo_replset_add_seed( conn, TEST_SERVER, 30001 );

    if( res = mongo_replset_connect( conn ) ) {
        mongo_destroy( conn );
        return res;
    }
    else {
      fprintf( stderr, "Disconnect now:\n");
      sleep( 10 );
    do {
        MONGO_TRY {
          e = 1;
          res = mongo_find_one( conn, "foo.bar", bson_empty(&b), bson_empty(&b), NULL);
          e = 0;
        } MONGO_CATCH {
          sleep( 2 );
          if( e++ < 30) {
            fprintf( stderr, "Attempting reconnect %d.\n", e);
            mongo_reconnect( conn );
          } else {
            fprintf( stderr, "Fail.\n");
            return -1;
          }
        }
    } while(e);
  }

  return 0;
}

int main() {
    ASSERT( test_connect( "test-rs" ) == 0 );
    ASSERT( test_connect( "test-foobar" ) == mongo_conn_bad_set_name );

    /* Run this for testing failover.
    ASSERT( test_reconnect( "test-rs" ) == 0 );
    */

    return 0;
}

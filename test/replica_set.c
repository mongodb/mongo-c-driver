/* test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef SEED_START_PORT
#define SEED_START_PORT 30000
#endif

int test_connect( const char* set_name ) {

    mongo_connection conn[1];
    int res;

    INIT_SOCKETS_FOR_WINDOWS;

    mongo_replset_init_conn( conn, set_name );
    mongo_replset_add_seed( conn, TEST_SERVER, SEED_START_PORT + 1 );
    mongo_replset_add_seed( conn, TEST_SERVER, SEED_START_PORT );

    if( (res = mongo_replset_connect( conn )) ) {
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
    mongo_replset_add_seed( conn, TEST_SERVER, SEED_START_PORT );
    mongo_replset_add_seed( conn, TEST_SERVER, SEED_START_PORT + 1 );

    if( (res = mongo_replset_connect( conn )) ) {
        mongo_destroy( conn );
        return res;
    }
    else {
      fprintf( stderr, "Disconnect now:\n");
      sleep( 10 );
      e = 1;
    do {
          res = mongo_find_one( conn, "foo.bar", bson_empty(&b), bson_empty(&b), NULL);
          if( res == MONGO_ERROR && conn->err == MONGO_IO_ERROR ) {
            sleep( 2 );
            if( e++ < 30) {
              fprintf( stderr, "Attempting reconnect %d.\n", e);
              mongo_reconnect( conn );
            } else {
              fprintf( stderr, "Fail.\n");
              return -1;
            }
          }
    } while(1);
  }

  return 0;
}

int main() {
    ASSERT( test_connect( "test-rs" ) == 0 );
    ASSERT( test_connect( "test-foobar" ) == mongo_conn_bad_set_name );

    /*
    ASSERT( test_reconnect( "test-rs" ) == 0 );
    */

    return 0;
}

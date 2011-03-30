/* test.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    mongo_connection conn[1];

    INIT_SOCKETS_FOR_WINDOWS;

    mongo_replset_init_conn( conn );
    mongo_replset_add_seed( conn, TEST_SERVER, 30000 );
    mongo_replset_add_seed( conn, TEST_SERVER, 30001 );
    mongo_host_port* p = conn->seeds;

    while( p != NULL ) {
      p = p->next;
    }

    if( mongo_replset_connect( conn ) ) {
      printf("Failed to connect.");
      exit(1);
    }

    return 0;
}

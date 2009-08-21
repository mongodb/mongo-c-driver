/* test.c */

#include "mongo.h"
#include <stdio.h>

int main(){
    struct mongo_connection conn;
    int i;
    
    mongo_exit_on_error( mongo_connect( &conn , 0 ) );

    for ( i=0; i<10; i++ ){
        printf( "%d\n" , i );
    }
}

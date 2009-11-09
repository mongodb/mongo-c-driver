/* mongo.c */

#include "mongo.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

/* ----------------------------
   message stuff
   ------------------------------ */

mongo_message * mongo_message_create( int len , int id , int responseTo , int op ){
    mongo_message * mm = (mongo_message*)malloc( len );
    if ( ! mm )
        return 0;
    mm->len = len;
    if ( id )
        mm->id = id;
    else
        mm->id = rand();
    mm->responseTo = responseTo;
    mm->op = op;
    return mm;
}

/* ----------------------------
   connection stuff
   ------------------------------ */

int mongo_connect( mongo_connection * conn , mongo_connection_options * options ){
    int x = 1;
    conn->options.port = 27017;
    conn->connected = 0;

    if ( options ){
        memcpy( &(conn->options) , options , sizeof( mongo_connection_options ) );
        printf( "can't handle options to mongo_connect yet" );
        exit(-2);
        return -2;
    }
    else {
        strcpy( conn->options.host , "127.0.0.1" );
    }

    /* setup */

    conn->sock = 0;

    memset( conn->sa.sin_zero , 0 , sizeof(conn->sa.sin_zero) );
    conn->sa.sin_family = AF_INET;
    conn->sa.sin_port = htons(conn->options.port);
    conn->sa.sin_addr.s_addr = inet_addr( conn->options.host );
    conn->addressSize = sizeof(conn->sa);

    /* connect */
    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock <= 0 ){
        fprintf( stderr , "couldn't get socket errno: %d" , errno );
        return -1;
    }

    if ( connect( conn->sock , (struct sockaddr*)&conn->sa , conn->addressSize ) ){
        fprintf( stderr , "couldn' connect errno: %d\n" , errno );
        return -2;
    }

    /* options */

    /* nagle */
    if ( setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &x, sizeof(x) ) ){
        fprintf( stderr , "disbale nagle failed" );
        return -3;
    }

    /* TODO signals */


    conn->connected = 1;
    return 0;
}

int mongo_insert( mongo_connection * conn , const char * ns , bson * bson ){
    char * data;
    mongo_message * mm = mongo_message_create( 16 + 4 + strlen( ns ) + 1 + bson_size( bson ) ,
                                                      0 , 0 , mongo_op_insert );
    if ( ! mm )
        return 0;

    data = &mm->data;
    memset( data , 0 , 4 );
    memcpy( data + 4 , ns , strlen( ns ) + 1 );
    memcpy( data + 4 + strlen( ns ) + 1 , bson->data , bson_size( bson ) );

    write( conn->sock , mm , mm->len );
    return 0;
}

char * mongo_data_append( char * start , const void * data , int len ){
    memcpy( start , data , len );
    return start + len;
}

mongo_message * mongo_read_response( mongo_connection * conn ){
    char smallbuf[16];
    mongo_message * mm;
    int total , temp;
    
    total = 0;
    while ( total < 16 ){
        temp = read( conn->sock , smallbuf , 16 - total );
        if ( temp < 0 ){
            fprintf( stderr , "error reading " );
            return 0;
        }
    }
}

void mongo_query( mongo_connection * conn , const char * ns , bson * query , bson * fields , int nToReturn , int nToSkip , int options ){
    char * data;
    mongo_message * mm = mongo_message_create( 16 + 
                                                      4 + /*  options */
                                                      strlen( ns ) + 1 + /* ns */
                                                      4 + 4 + /* skip,return */
                                                      bson_size( query ) +
                                                      bson_size( fields ) ,
                                                      0 , 0 , mongo_op_query );

    data = &mm->data;
    data = mongo_data_append( data , &options , 4 );
    data = mongo_data_append( data , ns , strlen( ns ) + 1 );    
    data = mongo_data_append( data , &nToSkip , 4 );
    data = mongo_data_append( data , &nToReturn , 4 );
    data = mongo_data_append( data , query->data , bson_size( query ) );    
    if ( fields )
        data = mongo_data_append( data , fields->data , bson_size( fields ) );    
    

    bson_fatal( "query building fail!" , data == ((char*)mm) + mm->len );

    write( conn->sock , mm , mm->len );
    
}

int mongo_disconnect( mongo_connection * conn ){
    if ( ! conn->connected )
        return 1;

    close( conn->sock );
    
    conn->sock = 0;
    conn->connected = 0;
    
    return 0;
}

int mongo_destory( mongo_connection * conn ){
    return mongo_disconnect( conn );
}

void mongo_exit_on_error( int ret ){
    if ( ret == 0 )
        return;

    printf( "unexpeted error: %d\n" , ret );
    exit(ret);
}

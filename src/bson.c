/* bson.c */

#include "bson.h"
#include <string.h>
#include <malloc.h>

const int initialBufferSize = 128;

void bson_init( struct bson_buffer * b ){
    b->buf = (char*)malloc( initialBufferSize );
    b->bufSize = initialBufferSize;
    b->cur = b->buf + 4;
    b->finished = 0;
}

void bson_ensure_space( struct bson_buffer * b , int bytesNeeded ){
    
}

void bson_finish( struct bson_buffer * b ){
    
}

void bson_append( struct bson_buffer * b , const void * data , int len ){
    memcpy( b->cur , data , len );
    b->cur += len;
}

void bson_append_byte( struct bson_buffer * b , char c ){
    b->cur[0] = c;
    b->cur++;
}

void bson_append_estart( struct bson_buffer * b , int type , const char * name , int dataSize ){
    bson_ensure_space( b , 1 + strlen( name ) + 1 + dataSize );
    bson_append_byte( b , (char)type );
    bson_append( b , name , strlen( name ) + 1 );
    
}

void bson_append_int( struct bson_buffer * b , const char * name , int i ){
    bson_append_estart( b , bson_int , name , 4 );
    bson_append( b , &i , 4 );
}
void bson_append_bool( struct bson_buffer * b , const char * name , int i ){
    bson_append_estart( b , bson_bool , name , 1 );
    bson_append_byte( b , i != 0 );
}
void bson_append_null( struct bson_buffer * b , const char * name ){
    bson_append_estart( b , bson_null , name , 0 );
}





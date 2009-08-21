/* bson.c */

#include "bson.h"
#include <string.h>
#include <malloc.h>

const int initialBufferSize = 128;

/* ----------------------------
   BUILDING
   ------------------------------ */

struct bson_buffer * bson_init( struct bson_buffer * b ){
    b->buf = (char*)malloc( initialBufferSize );
    if ( ! b->buf )
        return 0;
    b->bufSize = initialBufferSize;
    b->cur = b->buf + 4;
    b->finished = 0;
    return b;
}

void bson_append_byte( struct bson_buffer * b , char c ){
    b->cur[0] = c;
    b->cur++;
}
void bson_append( struct bson_buffer * b , const void * data , int len ){
    memcpy( b->cur , data , len );
    b->cur += len;
}

struct bson_buffer * bson_ensure_space( struct bson_buffer * b , int bytesNeeded ){
    if ( b->finished )
        return 0;
    /* TODO */
    return 0;
}

struct bson_buffer * bson_finish( struct bson_buffer * b ){
    int * i;
    if ( ! bson_ensure_space( b , 1 ) ) return 0;
    bson_append_byte( b , 0 );
    i = (int*)b->buf;
    i[0] = b->cur - b->buf;
    b->finished = 1;
    return b;
}

void bson_destroy( struct bson_buffer * b ){
    free( b->buf );
    b->buf = 0;
    b->cur = 0;
    b->finished = 1;
}

struct bson_buffer * bson_append_estart( struct bson_buffer * b , int type , const char * name , int dataSize ){
    if ( ! bson_ensure_space( b , 1 + strlen( name ) + 1 + dataSize ) )
        return 0;
    bson_append_byte( b , (char)type );
    bson_append( b , name , strlen( name ) + 1 );
    return b;
}

/* ----------------------------
   BUILDING TYPES
   ------------------------------ */

struct bson_buffer * bson_append_int( struct bson_buffer * b , const char * name , int i ){
    if ( ! bson_append_estart( b , bson_int , name , 4 ) ) return 0;
    bson_append( b , &i , 4 );
    return b;
}
struct bson_buffer * bson_append_bool( struct bson_buffer * b , const char * name , int i ){
    if ( ! bson_append_estart( b , bson_bool , name , 1 ) ) return 0;
    bson_append_byte( b , i != 0 );
    return b;
}
struct bson_buffer * bson_append_null( struct bson_buffer * b , const char * name ){
    if ( ! bson_append_estart( b , bson_null , name , 0 ) ) return 0;
    return b;
}





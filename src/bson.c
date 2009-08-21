/* bson.c */

#include "bson.h"
#include <stdlib.h>
#include <string.h>

const int initialBufferSize = 128;

/* ----------------------------
   READING
   ------------------------------ */

struct bson * bson_init( struct bson * b , char * data , int mine ){
    b->data = data;
    b->owned = mine;
    return b;
}
int bson_size( struct bson * b ){
    int * i = (int*)b->data;
    return i[0];
}
void bson_destory( struct bson * b ){
    if ( b->owned && b->data )
        free( b->data );
    b->data = 0;
    b->owned = 0;
}

void bson_print( struct bson * b ){

}


/* ----------------------------
   BUILDING
   ------------------------------ */

struct bson_buffer * bson_buffer_init( struct bson_buffer * b ){
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

struct bson_buffer * bson_ensure_space( struct bson_buffer * b , const int bytesNeeded ){
    if ( b->finished )
        return 0;
    if ( b->bufSize - ( b->cur - b->buf ) > bytesNeeded )
        return b;
    b->buf = (char*)realloc( b->buf , (int)(1.5 * ( b->bufSize + bytesNeeded ) ) );
    if ( ! b->buf )
        return 0;
    return b;
}

char * bson_finish( struct bson_buffer * b ){
    int * i;
    if ( ! bson_ensure_space( b , 1 ) ) return 0;
    bson_append_byte( b , 0 );
    i = (int*)b->buf;
    i[0] = b->cur - b->buf;
    b->finished = 1;
    return b->buf;
}

void bson_destroy( struct bson_buffer * b ){
    free( b->buf );
    b->buf = 0;
    b->cur = 0;
    b->finished = 1;
}

struct bson_buffer * bson_append_estart( struct bson_buffer * b , int type , const char * name , const int dataSize ){
    if ( ! bson_ensure_space( b , 1 + strlen( name ) + 1 + dataSize ) )
        return 0;
    bson_append_byte( b , (char)type );
    bson_append( b , name , strlen( name ) + 1 );
    return b;
}

/* ----------------------------
   BUILDING TYPES
   ------------------------------ */

struct bson_buffer * bson_append_int( struct bson_buffer * b , const char * name , const int i ){
    if ( ! bson_append_estart( b , bson_int , name , 4 ) ) return 0;
    bson_append( b , &i , 4 );
    return b;
}
struct bson_buffer * bson_append_double( struct bson_buffer * b , const char * name , const double d ){
    if ( ! bson_append_estart( b , bson_double , name , 8 ) ) return 0;
    bson_append( b , &d , 8 );
    return b;
}
struct bson_buffer * bson_append_bool( struct bson_buffer * b , const char * name , const int i ){
    if ( ! bson_append_estart( b , bson_bool , name , 1 ) ) return 0;
    bson_append_byte( b , i != 0 );
    return b;
}
struct bson_buffer * bson_append_null( struct bson_buffer * b , const char * name ){
    if ( ! bson_append_estart( b , bson_null , name , 0 ) ) return 0;
    return b;
}





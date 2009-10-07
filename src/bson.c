/* bson.c */

#include "bson.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    int * i;
    if ( ! b || ! b->data )
        return 0;
    i = (int*)b->data;
    return i[0];
}
void bson_destory( struct bson * b ){
    if ( b->owned && b->data )
        free( b->data );
    b->data = 0;
    b->owned = 0;
}

void bson_print( struct bson * b ){
    bson_print_raw( b->data , 0 );
}

void bson_print_raw( const char * data , int depth ){
    struct bson_iterator i;
    const char * key;
    int temp;
    bson_iterator_init( &i , data );

    while ( bson_iterator_more( &i ) ){
        enum bson_type t = bson_iterator_next( &i );
        if ( t == 0 )
            break;
        key = bson_iterator_key( &i );
        
        for ( temp=0; temp<=depth; temp++ )
            printf( "\t" );
        printf( "%s : %d \t " , key , t );
        switch ( t ){
        case bson_int: printf( "%d" , bson_iterator_int( &i ) ); break;
        case bson_double: printf( "%f" , bson_iterator_double( &i ) ); break;
        case bson_bool: printf( "%s" , bson_iterator_bool( &i ) ? "true" : "false" ); break;
        case bson_string: printf( "%s" , bson_iterator_string( &i ) ); break;
        case bson_null: printf( "null" ); break;
        case bson_object:
        case bson_array:
            printf( "\n" );
            bson_print_raw( bson_iterator_value( &i ) , depth + 1 );
            break;
        default:
            fprintf( stderr , "can't print type : %d\n" , t );
        }
        printf( "\n" );
    }
}

/* ----------------------------
   ITERATOR
   ------------------------------ */

void bson_iterator_init( struct bson_iterator * i , const char * bson ){
    i->cur = bson + 4;
    i->first = 1;
}

int bson_iterator_more( struct bson_iterator * i ){
    return *(i->cur);
}

enum bson_type bson_iterator_next( struct bson_iterator * i ){
    int ds;

    if ( i->first ){
        i->first = 0;
        return (enum bson_type)(*i->cur);
    }
    
    switch ( (enum bson_type)(*i->cur) ){
    case bson_double: ds = 8; break;
    case bson_bool: ds = 1; break;
    case bson_null: ds = 0; break;
    case bson_int: ds = 4; break;
    case bson_long: ds = 8; break;
    case bson_string: ds = 4 + ((int*)bson_iterator_value(i))[0]; break;
    case bson_object: ds = ((int*)bson_iterator_value(i))[0]; break;
    case bson_array: ds = ((int*)bson_iterator_value(i))[0]; break;
    default: 
        fprintf( stderr , "WTF: %d\n"  , (int)(i->cur[0]) );
        exit(-1);
        return 0;
    }
    
    i->cur += 1 + strlen( i->cur + 1 ) + 1 + ds;

    return (enum bson_type)(*i->cur);
}

const char * bson_iterator_key( struct bson_iterator * i ){
    return i->cur + 1;
}
const char * bson_iterator_value( struct bson_iterator * i ){
    const char * t = i->cur + 1;
    t += strlen( t ) + 1;
    return t;
}

/* types */

int bson_iterator_int( struct bson_iterator * i ){
    return ((int*)bson_iterator_value( i ))[0];
}
double bson_iterator_double( struct bson_iterator * i ){
    return ((double*)bson_iterator_value( i ))[0];
}

int bson_iterator_bool( struct bson_iterator * i ){
    return bson_iterator_value( i )[0];
}
const char * bson_iterator_string( struct bson_iterator * i ){
    return bson_iterator_value( i ) + 4;
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
    b->stackPos = 0;
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
    if ( ! b->finished ){
        if ( ! bson_ensure_space( b , 1 ) ) return 0;
        bson_append_byte( b , 0 );
        i = (int*)b->buf;
        i[0] = b->cur - b->buf;
        b->finished = 1;
    }
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
struct bson_buffer * bson_append_string( struct bson_buffer * b , const char * name , const char * value ){
    int sl = strlen( value ) + 1;
    if ( ! bson_append_estart( b , bson_string , name , 4 + sl ) ) return 0;
    bson_append( b , &sl , 4 );
    bson_append( b , value , sl );
    return b;
}


struct bson_buffer * bson_append_start_object( struct bson_buffer * b , const char * name ){
    int x = 0;
    if ( ! bson_append_estart( b , bson_object , name , 5 ) ) return 0;
    b->stack[ b->stackPos++ ] = b->cur;
    bson_append( b , &x , 4 );
    return b;
}

struct bson_buffer * bson_append_start_array( struct bson_buffer * b , const char * name ){
    int x = 0;
    if ( ! bson_append_estart( b , bson_array , name , 5 ) ) return 0;
    b->stack[ b->stackPos++ ] = b->cur;
    bson_append( b , &x , 4 );
    return b;
}

struct bson_buffer * bson_append_finish_object( struct bson_buffer * b ){
    char * start;
    int i;
    if ( ! bson_ensure_space( b , 1 ) ) return 0;
    bson_append_byte( b , 0 );
    
    start = b->stack[ --b->stackPos ];
    i = b->cur - start;
    memcpy( start , &i , 4 );

    return b;
}


void bson_fatal( char * msg , int ok ){
    if ( ok )
        return;
    fprintf( stderr , "bson error: %s\n" , msg );
    exit(-5);
}

/* bson.h */

#ifndef _BSON_H_
#define _BSON_H_

enum bson_type {
    bson_eoo=0 ,
    bson_double=1,
    bson_string=2,
    bson_object=3,
    bson_array=4,
    bson_bindata=5,
    bson_undefined=6,
    bson_oid=7,
    bson_bool=8,
    bson_date=9,
    bson_null=10,
    bson_regex=11,
    bson_dbref=12,
    bson_code=13,
    bson_symbol=14,
    bson_codewscope=15,
    bson_int = 16,
    bson_timestamp = 17,
    bson_long = 18
};

/* ----------------------------
   READING
   ------------------------------ */

struct bson {
    char * data;
    int owned;
};

struct bson * bson_init( struct bson * b , char * data , int mine );
int bson_size( struct bson * b );
void bson_destory( struct bson * b );

void bson_print( struct bson * b );


/* ----------------------------
   BUILDING
   ------------------------------ */

struct bson_buffer {
    char * buf;
    char * cur;
    int bufSize;
    int finished;
};

struct bson_buffer * bson_buffer_init( struct bson_buffer * b );
struct bson_buffer * bson_ensure_space( struct bson_buffer * b , const int bytesNeeded );

/**
 * @return the raw data.  you either should free this OR call bson_destory not both
 */
char * bson_finish( struct bson_buffer * b );
void bson_destroy( struct bson_buffer * b );

struct bson_buffer * bson_append_int( struct bson_buffer * b , const char * name , const int i );
struct bson_buffer * bson_append_double( struct bson_buffer * b , const char * name , const double d );
struct bson_buffer * bson_append_string( struct bson_buffer * b , const char * name , const char * str );
struct bson_buffer * bson_append_bool( struct bson_buffer * b , const char * name , const int i );
struct bson_buffer * bson_append_null( struct bson_buffer * b , const char * name );

void bson_fatal( char * msg , int ok );

#endif

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


struct bson_buffer {
    char * buf;
    char * cur;
    int bufSize;
    int finished;
};

void bson_init( struct bson_buffer * b );
void bson_ensure_space( struct bson_buffer * b , int bytesNeeded );
void bson_finish( struct bson_buffer * b );

void bson_append_int( struct bson_buffer * b , const char * name , int i );
void bson_append_string( struct bson_buffer * b , const char * name , const char * str );
void bson_append_bool( struct bson_buffer * b , const char * name , int i );
void bson_append_null( struct bson_buffer * b , const char * name );


#endif

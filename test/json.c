/* testjson.c */

#include <stdio.h>

#include "mongo.h"
#include "json/json.h"

char * json_to_bson( char * js ){
    struct json_object * o = json_tokener_parse(js);
    struct bson_buffer bb;
    
    if ( is_error( o ) )
        return 0;
    
    if ( ! json_object_is_type( o , json_type_object ) ){
        fprintf( stderr , "json_to_bson needs a JSON object, not type\n" );
        return 0;
    }
    
    bson_buffer_init( &bb );
    json_object_object_foreach(o,k,v){
        if ( v ){
            switch ( json_object_get_type( v ) ){
            case json_type_int:
                bson_append_int( &bb , k , json_object_get_int( v ) );
                break;
            case json_type_boolean:
                bson_append_bool( &bb , k , json_object_get_boolean( v ) );
                break;
            case json_type_double:
                bson_append_double( &bb , k , json_object_get_double( v ) );
                break;
            case json_type_string:
                bson_append_string( &bb , k , json_object_get_string( v ) );
                break;
            default:
                fprintf( stderr , "can't handle type for : %s\n" , json_object_to_json_string(v) );
                return 0;
            }
        }
        else {
            bson_append_null( &bb , k );
        }
    }
    return bson_finish( &bb );
}

int json_to_bson_test( char * js , int size , const char * hash ){
    struct bson b;

    bson_init( &b , json_to_bson( js ) , 1 );

    if ( b.data == 0 ){
        if ( size == 0 )
            return 1;
        fprintf( stderr , "error: %s\n" , js );
        return 0;
        
    }
    
    if ( size != bson_size( &b ) ){
        fprintf( stderr , "sizes don't match [%s] want != got %d != %d\n" , js , size , bson_size(&b) );
        bson_destory( &b );
        return 0;
    }    

    fprintf( stderr , "%s\n" , js );
    bson_print( &b );

    bson_destory( &b );
    return 1;
}

int total = 0;
int fails = 0;

int run_json_to_bson_test( char * js , int size , const char * hash ){
    total++;
    if ( ! json_to_bson_test( js , size , hash ) )
        fails++;
    
    return fails;
}

int main(){

    run_json_to_bson_test( "1" , 0 , 0 );

    run_json_to_bson_test( "{ 'x' : true }" , 9 , "" );
    run_json_to_bson_test( "{ 'x' : null }" , 8 , "" );
    run_json_to_bson_test( "{ 'x' : 5.2 }" , 16 , "" );
    run_json_to_bson_test( "{ 'x' : 4 }" , 12 , "" );
    run_json_to_bson_test( "{ 'x' : 'eliot' }" , 18 , "" );
    run_json_to_bson_test( "{ 'x' : 5.2 , 'y' : 'truth' , 'z' : 1 }" , 36 , "" );
    run_json_to_bson_test( "{ 'x' : 5.2 , 'y' : 'truth' , 'z' : 1.1 }" , 40 , "" );
    run_json_to_bson_test( "{ 'x' : 'eliot' , 'y' : true , 'z' : 1 }" , 29 , "" );
    run_json_to_bson_test( "{ 'x' : 5.2 , 'y' : { 'a' : 'eliot' , b : true } , 'z' : null }" , 44 , "" );
    run_json_to_bson_test( "{ 'x' : 5.2 , 'y' : [ 'a' , 'eliot' , 'b' , true ] , 'z' : null }" , 62 , "" );
    
    printf( "----\ntotal: %d\nfails : %d\n" , total , fails );
    
    return fails;
}

/* functions.c */

#include "test.h"
#include "mongo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

int test_value = 0;

void *my_malloc( size_t size ) {
    test_value = 1;
    return malloc( size );
}

void *my_realloc( void *ptr, size_t size ) {
    test_value = 2;
    return realloc( ptr, size );
}

void my_free( void *ptr ) {
    test_value = 3;
    free( ptr );
}

int my_printf( const char *format, va_list ap ) {
    int ret;
    test_value = 4;

    ret = vprintf( format, ap );
    return ret;
}

int my_fprintf( FILE *fp, const char *format, va_list ap ) {
    int ret;
    test_value = 5;

    ret = vfprintf( fp, format, ap );
    return ret;
}

int my_sprintf( char *s, const char *format, va_list ap ) {
    int ret;
    test_value = 6;

    ret = vsprintf( s, format, ap );
    return ret;
}

int main() {

    void *ptr;
    char str[32];
    int size = 256;

    ptr = bson_malloc( size );
    ASSERT( test_value == 0 );
    ptr = bson_realloc( ptr, size + 64 );
    ASSERT( test_value == 0 );
    bson_free( ptr );
    ASSERT( test_value == 0 );

    bson_set_malloc( my_malloc );
    bson_set_realloc( my_realloc );
    bson_set_free( my_free );

    ptr = bson_malloc( size );
    ASSERT( test_value == 1 );
    ptr = bson_realloc( ptr, size + 64 );
    ASSERT( test_value == 2 );
    bson_free( ptr );
    ASSERT( test_value == 3 );

    test_value = 0;

    bson_printf( "Test %d\n", test_value );
    ASSERT( test_value == 0 );
    bson_fprintf( stdout, "Test %d\n", test_value );
    ASSERT( test_value == 0 );
    bson_sprintf( str, "Test %d\n", test_value );
    bson_printf( "Str: %s\n", str );
    ASSERT( test_value == 0 );

    bson_set_printf( my_printf );
    bson_set_fprintf( my_fprintf );
    bson_set_sprintf( my_sprintf );

    bson_printf( "Test %d\n", test_value );
    ASSERT( test_value == 4 );
    bson_fprintf( stdout, "Test %d\n", test_value );
    ASSERT( test_value == 5 );
    bson_sprintf( str, "Test %d\n", test_value );
    ASSERT( test_value == 6 );
    bson_printf( "Str: %s\n", str );

    return 0;
}

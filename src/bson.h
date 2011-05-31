/**
 * @file bson.h
 * @brief BSON Declarations
 */

/*    Copyright 2009, 2010, 2011 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _BSON_H_
#define _BSON_H_

#include "platform_hacks.h"
#include <time.h>

/* Generic error and warning flags. */
#define BSON_OK 0
#define BSON_ERROR -1
#define BSON_WARNING -2

/* BSON validity flags. */
#define BSON_VALID 0x0
#define BSON_NOT_UTF8 0x2 /**< Either a key or a string is not valid UTF-8. */
#define BSON_FIELD_HAS_DOT 0x4 /**< Warning: key contains '.' character. */
#define BSON_FIELD_INIT_DOLLAR 0x8 /**< Warning: key starts with '$' character. */

/* BSON error codes. */
#define BSON_OBJECT_FINISHED 1 /**< Trying to modify a finished BSON object. */

MONGO_EXTERN_C_START

typedef enum {
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
    bson_dbref=12, /* deprecated */
    bson_code=13,
    bson_symbol=14,
    bson_codewscope=15,
    bson_int = 16,
    bson_timestamp = 17,
    bson_long = 18
} bson_type;

typedef int bson_bool_t;

typedef struct {
    char * data;
    bson_bool_t owned;
    int err; /**< Bitfield representing errors or warnings on this bson object. */
    char* errstr; /**< A string representation of the most recent error or warning. */
} bson;

typedef struct {
    const char * cur;
    bson_bool_t first;
} bson_iterator;

typedef struct {
    char * buf;
    char * cur;
    int bufSize;
    bson_bool_t finished;
    int stack[32];
    int stackPos;
    int err; /**< Bitfield representing errors or warnings on this buffer */
    char* errstr; /**< A string representation of the most recent error or warning. */
} bson_buffer;

#pragma pack(1)
typedef union{
    char bytes[12];
    int ints[3];
} bson_oid_t;
#pragma pack()

typedef int64_t bson_date_t; /* milliseconds since epoch UTC */

typedef struct {
  int i; /* increment */
  int t; /* time in seconds */
} bson_timestamp_t;

/* ----------------------------
   READING
   ------------------------------ */
/**
 * Returns a pointer to a static empty BSON object.
 *
 * @param obj the BSON object to initialize.
 *
 * @return the empty initialized BSON object.
 */
bson * bson_empty(bson * obj); /* returns pointer to static empty bson object */

/**
 * Copy BSON data from one object to another.
 *
 * @param out the copy destination BSON object.
 * @param in the copy source BSON object.
 */
void bson_copy(bson* out, const bson* in); /* puts data in new buffer. NOOP if out==NULL */

/**
 * Make a BSON object from a BSON buffer.
 *
 * @param b the destination BSON object.
 * @param buf the source BSON buffer object.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_from_buffer(bson * b, bson_buffer * buf);

/**
 * Initialize a BSON object.
 *
 * @param b the BSON object to initialize.
 * @param data the raw BSON data.
 * @param mine whether or not the data's allocation should be freed by bson_destroy
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_init( bson * b , char * data , bson_bool_t mine );

/**
 * Size of a BSON object.
 *
 * @param b the BSON object.
 *
 * @return the size.
 */
int bson_size(const bson * b );

/**
 * Destroy a BSON object.
 *
 * @param b the object to destroy.
 */
void bson_destroy( bson * b );

/**
 * Print a string representation of a BSON object.
 *
 * @param b the BSON object to print.
 */
void bson_print( bson * b );

/**
 * Print a string representation of a BSON object.
 *
 * @param bson the raw data to print.
 * @param depth the depth to recurse the object.x
 */
void bson_print_raw( const char * bson , int depth );

/* advances iterator to named field */
/* returns bson_eoo (which is false) if field not found */
/**
 * Advance a bson_iterator to the named field.
 *
 * @param it the bson_iterator to use.
 * @param obj the BSON object to use.
 * @param name the name of the field to find.
 *
 * @return the type of the found object, bson_eoo if it is not found.
 */
bson_type bson_find(bson_iterator* it, const bson* obj, const char* name);

/**
 * Initialize a bson_iterator.
 *
 * @param i the bson_iterator to initialize.
 * @param bson the BSON object to associate with the iterator.
 */
void bson_iterator_init( bson_iterator * i , const char * bson );

/* more returns true for eoo. best to loop with bson_iterator_next(&it) */
/**
 * Check to see if the bson_iterator has more data.
 *
 * @param i the iterator.
 *
 * @return  returns true if there is more data.
 */
bson_bool_t bson_iterator_more( const bson_iterator * i );

/**
 * Point the iterator at the next BSON object.
 *
 * @param i the bson_iterator.
 *
 * @return the type of the next BSON object.
 */
bson_type bson_iterator_next( bson_iterator * i );

/**
 * Get the type of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the type of the current BSON object.
 */
bson_type bson_iterator_type( const bson_iterator * i );

/**
 * Get the key of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the key of the current BSON object.
 */
const char * bson_iterator_key( const bson_iterator * i );

/**
 * Get the value of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
const char * bson_iterator_value( const bson_iterator * i );

/* these convert to the right type (return 0 if non-numeric) */
/**
 * Get the double value of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
double bson_iterator_double( const bson_iterator * i );

/**
 * Get the int value of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
int bson_iterator_int( const bson_iterator * i );

/**
 * Get the long value of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
int64_t bson_iterator_long( const bson_iterator * i );

/* return the bson timestamp as a whole or in parts */
/**
 * Get the timestamp value of the BSON object currently pointed to by
 * the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
bson_timestamp_t bson_iterator_timestamp( const bson_iterator * i );

/**
 * Get the boolean value of the BSON object currently pointed to by
 * the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
/* false: boolean false, 0 in any type, or null */
/* true: anything else (even empty strings and objects) */
bson_bool_t bson_iterator_bool( const bson_iterator * i );

/**
 * Get the double value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
/* these assume you are using the right type */
double bson_iterator_double_raw( const bson_iterator * i );

/**
 * Get the int value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
int bson_iterator_int_raw( const bson_iterator * i );

/**
 * Get the long value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
int64_t bson_iterator_long_raw( const bson_iterator * i );

/**
 * Get the bson_bool_t value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
bson_bool_t bson_iterator_bool_raw( const bson_iterator * i );

/**
 * Get the bson_oid_t value of the BSON object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
bson_oid_t* bson_iterator_oid( const bson_iterator * i );

/**
 * Get the string value of the BSON object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
/* these can also be used with bson_code and bson_symbol*/
const char * bson_iterator_string( const bson_iterator * i );

/**
 * Get the string length of the BSON object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the length of the current BSON object.
 */
int bson_iterator_string_len( const bson_iterator * i );

/**
 * Get the code value of the BSON object currently pointed to by the
 * iterator. Works with bson_code, bson_codewscope, and bson_string 
 * returns NULL for everything else.
 *
 * @param i the bson_iterator
 *
 * @return the code value of the current BSON object.
 */
/* works with bson_code, bson_codewscope, and bson_string */
/* returns NULL for everything else */
const char * bson_iterator_code(const bson_iterator * i);

/**
 * Calls bson_empty on scope if not a bson_codewscope 
 *
 * @param i the bson_iterator.
 * @param scope the bson scope.
 */
/* calls bson_empty on scope if not a bson_codewscope */
void bson_iterator_code_scope(const bson_iterator * i, bson * scope);

/**
 * Get the date value of the BSON object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the date value of the current BSON object.
 */
/* both of these only work with bson_date */
bson_date_t bson_iterator_date(const bson_iterator * i);

/**
 * Get the time value of the BSON object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the time value of the current BSON object.
 */
time_t bson_iterator_time_t(const bson_iterator * i);

/**
 * Get the length of the BSON binary object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the length of the current BSON binary object.
 */
int bson_iterator_bin_len( const bson_iterator * i );

/**
 * Get the type of the BSON binary object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the type of the current BSON binary object.
 */
char bson_iterator_bin_type( const bson_iterator * i );

/**
 * Get the value of the BSON binary object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON binary object.
 */
const char * bson_iterator_bin_data( const bson_iterator * i );

/**
 * Get the value of the BSON regex object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON regex object.
 */
const char * bson_iterator_regex( const bson_iterator * i );

/**
 * Get the options of the BSON regex object currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator.
 *
 * @return the options of the current BSON regex object.
 */
const char * bson_iterator_regex_opts( const bson_iterator * i );

/* these work with bson_object and bson_array */
/**
 * Get the BSON subobject currently pointed to by the
 * iterator. 
 *
 * @param i the bson_iterator.
 * @param sub the BSON subobject destination.
 */
void bson_iterator_subobject(const bson_iterator * i, bson * sub);

/**
 * Get a bson_iterator that on the BSON subobject.
 *
 * @param i the bson_iterator.
 * @param sub the iterator to point at the BSON subobject.
 */
void bson_iterator_subiterator(const bson_iterator * i, bson_iterator * sub);

/* str must be at least 24 hex chars + null byte */
/**
 * Create a bson_oid_t from a string. 
 *
 * @param oid the bson_oid_t destination.
 * @param str a null terminated string comprised of at least 24 hex chars.
 */
void bson_oid_from_string(bson_oid_t* oid, const char* str);

/**
 * Create a string representation of the bson_oid_t.
 *
 * @param oid the bson_oid_t source.
 * @param str the string representation destination.
 */
void bson_oid_to_string(const bson_oid_t* oid, char* str);

/**
 * Create a bson_oid object.
 *
 * @param oid the destination for the newly created bson_oid_t.
 */
void bson_oid_gen(bson_oid_t* oid);

/**
 * Get the time a bson_oid_t was created.
 *
 * @param oid the bson_oid_t.
 */
time_t bson_oid_generated_time(bson_oid_t* oid); /* Gives the time the OID was created */

/* ----------------------------
   BUILDING
   ------------------------------ */
/**
 * Initialize a bson_buffer.
 * 
 * @param b the bson_buffer object to initialize.
 *
 * @return 0. Exits if cannot allocate memory.
 */
int bson_buffer_init( bson_buffer * b );

/**
 * Grow a bson_buffer object.
 * 
 * @param b the bson_buffer to grow.
 * @param bytesNeeded the additional number of bytes needed.
 *
 * @return BSON_OK or BSON_ERROR with the bson_buffer error object set.
 *   Exits if allocation fails.
 */
int bson_ensure_space( bson_buffer * b, const int bytesNeeded );

/**
 * Finalize a bson_buffer object.
 *
 * @param b the bson_buffer object to finalize.
 *
 * @return the standard error code. To deallocate memory,
 *   call bson_buffer_destroy on the bson_buffer object.
 */
int bson_buffer_finish( bson_buffer * b );

/** 
 * Destroy a bson_buffer object.
 *
 * @param b the bson_buffer to destroy. 
 *
 */
void bson_buffer_destroy( bson_buffer * b );

/**
 * Append a previously created bson_oid_t to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the bson_oid_t.
 * @param oid the bson_oid_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_oid( bson_buffer * b, const char * name, const bson_oid_t* oid );

/**
 * Append a bson_oid_t to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the bson_oid_t.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_new_oid( bson_buffer * b, const char * name );

/**
 * Append an int to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the int.
 * @param i the int to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_int( bson_buffer * b, const char * name, const int i );

/**
 * Append an long to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the long.
 * @param i the long to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_long( bson_buffer * b, const char * name, const int64_t i );

/**
 * Append an double to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the double.
 * @param d the double to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_double( bson_buffer * b, const char * name, const double d );

/**
 * Append a string to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the string.
 * @param str the string to append.
 *
 * @return BSON_OK or BSON_ERROR.
*/
int bson_append_string( bson_buffer * b, const char * name, const char * str );

/**
 * Append len bytes of a string to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the string.
 * @param str the string to append.
 * @param len the number of bytes from str to append.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_string_n( bson_buffer * b, const char * name, const char * str, int len);

/**
 * Append a symbol to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the symbol.
 * @param str the symbol to append.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_symbol( bson_buffer * b, const char * name, const char * str );

/**
 * Append len bytes of a symbol to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the symbol.
 * @param str the symbol to append.
 * @param len the number of bytes from str to append.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_symbol_n( bson_buffer * b, const char * name, const char * str, int len );

/**
 * Append code to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the code.
 * @param str the code to append.
 * @param len the number of bytes from str to append.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_code( bson_buffer * b, const char * name, const char * str );

/**
 * Append len bytes of code to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the code.
 * @param str the code to append.
 * @param len the number of bytes from str to append.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_code_n( bson_buffer * b, const char * name, const char * str, int len );

/**
 * Append code to a bson_buffer with scope.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the code.
 * @param str the string to append.
 * @param scope a BSON object containing the scope.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_code_w_scope( bson_buffer * b, const char * name, const char * code, const bson * scope);

/**
 * Append len bytes of code to a bson_buffer with scope.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the code.
 * @param str the string to append.
 * @param len the number of bytes from str to append.
 * @param scope a BSON object containing the scope.
 * 
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_code_w_scope_n( bson_buffer * b, const char * name, const char * code, int size, const bson * scope);

/**
 * Append binary data to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the data.
 * @param type the binary data type.
 * @param str the binary data.
 * @param len the length of the data.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_binary( bson_buffer * b, const char * name, char type, const char * str, int len );

/**
 * Append a bson_bool_t to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the boolean value.
 * @param v the bson_bool_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_bool( bson_buffer * b, const char * name, const bson_bool_t v );

/**
 * Append a null value to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the null value.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_null( bson_buffer * b, const char * name );

/**
 * Append an undefined value to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the undefined value.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_undefined( bson_buffer * b, const char * name );

/**
 * Append a regex value to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the regex value.
 * @param pattern the regex pattern to append.
 * @param the regex options.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_regex( bson_buffer * b, const char * name, const char * pattern, const char * opts );

/**
 * Append bson data to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the bson data.
 * @param bson the bson object to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_bson( bson_buffer * b, const char * name, const bson* bson);

/**
 * Append a BSON element to a bson_buffer from the current point of an iterator.
 *
 * @param b the bson_buffer to append to.
 * @param name_or_null the key for the BSON element, or NULL.
 * @param elem the bson_iterator.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_element( bson_buffer * b, const char * name_or_null, const bson_iterator* elem);

/**
 * Append a bson_timestamp_t value to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the timestampe value.
 * @param ts the bson_timestamp_t value to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_timestamp( bson_buffer * b, const char * name, bson_timestamp_t * ts );

/* these both append a bson_date */
/**
 * Append a bson_date_t value to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the date value.
 * @param millis the bson_date_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_date(bson_buffer * b, const char * name, bson_date_t millis);

/**
 * Append a time_t value to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the key for the date value.
 * @param secs the time_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_time_t(bson_buffer * b, const char * name, time_t secs);

/**
 * Start appending a new object to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the name of the new object.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_start_object( bson_buffer * b, const char * name );

/**
 * Start appending a new array to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 * @param name the name of the new array.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_start_array( bson_buffer * b, const char * name );

/**
 * Finish appending a new object or array to a bson_buffer.
 *
 * @param b the bson_buffer to append to.
 *
 * @return BSON_OK or BSON_ERROR.
 */
int bson_append_finish_object( bson_buffer * b );

void bson_numstr(char* str, int i);
void bson_incnumstr(char* str);


/* ------------------------------
   ERROR HANDLING - also used in mongo code
   ------------------------------ */
/**
 * Allocates memory and checks return value, exiting fatally if malloc() fails.
 * 
 * @param size bytes to allocate.
 *
 * @return a pointer to the allocated memory.
 * 
 * @sa malloc(3)
 */ 
void * bson_malloc(int size); /* checks return value */

/**
 * Changes the size of allocated memory and checks return value, exiting fatally if realloc() fails.
 * 
 * @param size bytes to allocate.
 *
 * @return a pointer to the allocated memory.
 *
 * @sa malloc(3)
 */ 
void * bson_realloc(void * ptr, int size); /* checks return value */

/* bson_err_handlers shouldn't return!!! */
typedef void(*bson_err_handler)(const char* errmsg);

/* returns old handler or NULL */
/* default handler prints error then exits with failure*/
/**
 * Set a function for error handling.
 * 
 * @param func a bson_err_handler function.
 * 
 * @return the old error handling function, or NULL.
 */
bson_err_handler set_bson_err_handler(bson_err_handler func);

/* does nothing if ok != 0 */
/**
 * Exit fatally.
 *
 * @param ok exits if ok is equal to 0.
 */
void bson_fatal( int ok );

/**
 * Exit fatally with an error message.
  *
 * @param ok exits if ok is equal to 0.
 * @param msg prints to stderr before exiting.
 */
void bson_fatal_msg( int ok, const char* msg );

/**
 * Invoke the error handler but do not exit.
 *
 * @param b the buffer object.
 */
void bson_builder_error( bson_buffer* b );

MONGO_EXTERN_C_END
#endif

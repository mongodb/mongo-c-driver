/**
 * @file bcon.h
 * @brief BCON (BSON C Object Notation) Declarations
 */

/*    Copyright 2009-2012 10gen Inc.
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

#ifndef BCON_H_
#define BCON_H_

#include "bson.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

MONGO_EXTERN_C_START

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * BCON - BSON C Object Notation.
 *
 * Description
 * -----------
 * BCON provides for JSON-like (or BSON-like) initializers in C.
 * Without this, BSON must be constructed by procedural coding via explicit function calls.
 * With this, you now have convenient data-driven definition of BSON documents.
 * Here are a couple of introductory examples.
 *
 *     bcon hello[] = { "hello", "world", "." };
 *     bcon pi[] = { "pi", BF(3.14159), BEND };
 *
 * BCON is an array of bcon union elements with the default type of cstring (char *).
 * A BCON document must be terminated with a cstring containing a single dot, i.e., ".", or the macro equivalent BEND.
 *
 * Cstring literals in double quotes are used for keys as well as for string values.
 * There is no explicit colon (':') separator between key and value, just a comma,
 * however it must be explicit or C will quietly concatenate the key and value strings for you.
 * Readability may be improved by using multiple lines with a key-value pair per line.
 *
 * Macros are used to enclose specific types, and an internal type-specifier string prefixes a typed value.
 * Macros are also used to specify interpolation of values from pointers to specified types.
 *
 * Sub-documents are framed by "{" "}" string literals, and sub-arrays are framed by "[" "]" literals.
 *
 * All of this is needed because C arrays and initializers are mono-typed unlike dict/array types in modern languages.
 * BCON attempts to be readable and JSON-like within the context and restrictions of the C language.
 *
 * Examples
 * --------
 *
 *     bcon goodbye[] = { "hello", "world", "goodbye", "world", "." };
 *     bcon awesome[] = { "BSON", "[", "awesome", BF(5.05), BI(1986), "]", "." };
 *     bcon contact_info[] = {
 *         "firstName", "John",
 *         "lastName" , "Smith",
 *         "age"      , BI(25),
 *         "address"  ,
 *         "{",
 *             "streetAddress", "21 2nd Street",
 *             "city"         , "New York",
 *             "state"        , "NY",
 *             "postalCode"   , "10021",
 *         "}",
 *         "phoneNumber",
 *         "[",
 *             "{",
 *                 "type"  , "home",
 *                 "number", "212 555-1234",
 *             "}",
 *             "{",
 *                 "type"  , "fax",
 *                 "number", "646 555-4567",
 *             "}",
 *         "]",
 *         BEND
 *     };
 *
 * Comparison
 * ----------
 *
 *     JSON:
 *         { "BSON" : [ "awesome", 5.05, 1986 ] }
 *
 *     BCON:
 *         bcon awesome[] = { "BSON", "[", "awesome", BF(5.05), BI(1986), "]", BEND };
 *
 *     C driver calls:
 *         bson_init( b );
 *         bson_append_start_array( b, "BSON" );
 *         bson_append_string( b, "0", "awesome" );
 *         bson_append_double( b, "1", 5.05 );
 *         bson_append_int( b, "2", 1986 );
 *         bson_append_finish_array( b );
 *         ret = bson_finish( b );
 *         bson_print( b );
 *         bson_destroy( b );
 *
 * Peformance
 * ----------
 * BCON costs about three times as much as the equivalent bson function calls required to explicitly construct the document.
 * This is significantly less than the cost of parsing JSON and constructing BSON, and BCON allows value interpolation via pointers.
 *
 * Specification
 * -------------
 * This specification parallels the BSON specification - http://bsonspec.org/#/specification
 *
 *     document    ::= elist
 *     e_list      ::= element e_list
 *     element     ::= e_name value
 *     value       ::= cstring             String
 *                   | ":_f:" double       Floating point
 *                   | ":Pf:" *double      *Floating point interpolation
 *                   | ":_s" cstring       String
 *                   | ":Ps" *cstring      *String interpolation
 *                   | "{" document "}"    Embedded document
 *                   | ":PD" *document     *Embedded document interpolation
 *                   | "[" v_list "]"      Array
 *                   | ":PA" *v_list       *Array interpolation
 *                   | ":_b:" "\x00"       Boolean "false"
 *                   | ":_b:" "\x01"       Boolean "true"
 *                   | ":Pb:" *int         *Boolean interpolation
 *                   | ":_t:" long         UTC datetime
 *                   | ":Pt:" *long        *UTC datetime interpolation
 *                   | ":_v:" ""           Null value (empty string arg ignored)
 *                   | ":_x:" cstring      Symbol
 *                   | ":Px:" *cstring     *Symbol interpolation
 *                   | ":_i:" int          32-bit integer
 *                   | ":_i:" *int         *32-bit integer
 *                   | ":_l:" long         64-bit integer
 *                   | ":_l:" *long        *64-bit integer
 *     vlist       ::= value v_list
 *                   | ""
 *     e_name      ::= cstring
 *     cstring 	::= (byte*) "\x00"
 *
 * Notes
 * -----
 * Use the BS macro or the ":_s:" type specifier for string to allow string values that collide with type specifiers, braces, or square brackets.
 */

typedef union bcon {
    char *s;         /**< 02  e_name  string              UTF-8 string */
    char **Ps;       /**< 02  e_name  string              UTF-8 string interpolation */
    double f;        /**< 01  e_name  double              Floating point */
    double *Pf;      /**< 01  e_name  double              Floating point interpolation */
    union bcon *PD;  /**< 03  e_name  document            Embedded document interpolation */
    union bcon *PA;  /**< 04  e_name  document            Array interpolation */
    char *o;         /**< 07  e_name  (byte*12)           ObjectId */
    char **Po;       /**< 07  e_name  (byte*12)           ObjectId interpolation */
    bson_bool_t b;   /**< 08  e_name  00                  Boolean "false"
                          08  e_name  01                  Boolean "true" */
    bson_bool_t *Pb; /**< 08  e_name  01                  Boolean interpolation */
    time_t t;        /**< 09  e_name  int64               UTC datetime */
    time_t *Pt;      /**< 09  e_name  int64               UTC datetime interpolation */
    char *v;         /**< 0A  e_name                      Null value */
    char *x;         /**< 0E  e_name  string              Symbol */
    char **Px;       /**< 0E  e_name  string              Symbol interpolation */
    int i;           /**< 10  e_name  int32               32-bit Integer */
    int *Pi;         /**< 10  e_name  int32               32-bit Integer interpolation */
    long l;          /**< 12  e_name  int64               64-bit Integer */
    long *Pl;        /**< 12  e_name  int64               64-bit Integer interpolation */
    /* "{" "}" */    /*   03  e_name  document            Embedded document */
    /* "[" "]" */    /*   04  e_name  document            Array */
                     /*   05  e_name  binary              Binary data */
                     /*   06  e_name                      undefined - deprecated */
                     /*   0B  e_name  cstring cstring     Regular expression */
                     /*   0C  e_name  string (byte*12)    DBPointer - Deprecated */
                     /*   0D  e_name  string              JavaScript code */
                     /*   0F  e_name  code_w_s            JavaScript code w/ scope  */
                     /*   11  e_name  int64               Timestamp */
                     /*   FF  e_name                      Min key */
                     /*   7F  e_name                      Max key */
} bcon;

/** BCON document terminator */
#define BEND "."

/** BCON internal 02 cstring string type-specifier */
#define BTS ":_s:"
/** BCON internal 01 double Floating point type-specifier */
#define BTF ":_f:"
/** BCON internal 07 cstring ObjectId type-specifier */
#define BTO ":_o:"
/** BCON internal 08 int Boolean type-specifier */
#define BTB ":_b:"
/** BCON internal 09 int64 UTC datetime type-specifier */
#define BTT ":_t:"
/** BCON internal 0A Null type-specifier */
#define BTN ":_v:"
/** BCON internal 0E cstring Symbol type-specifier */
#define BTX ":_x:"
/** BCON internal 10 int32 64-bit Integer type-specifier */
#define BTI ":_i:"
/** BCON internal 12 int64 64-bit Integer type-specifier */
#define BTL ":_l:"

/** BCON internal 02 cstring* string interpolation type-specifier */
#define BTPS ":Ps:"
/** BCON internal 01 double* Floating point interpolation type-specifier */
#define BTPF ":Pf:"
/** BCON internal 07 cstring* ObjectId interpolation type-specifier */
#define BTPO ":Po:"
/** BCON internal 08 int* Boolean interpolation type-specifier */
#define BTPB ":Pb:"
/** BCON internal 09 int64* UTC datetime interpolation type-specifier */
#define BTPT ":Pt:"
/** BCON internal 0E cstring* Symbol interpolation type-specifier */
#define BTPX ":Px:"
/** BCON internal 10 int32* 64-bit Integer interpolation type-specifier */
#define BTPI ":Pi:"
/** BCON internal 12 int64* 64-bit Integer interpolation type-specifier */
#define BTPL ":Pl:"

/** BCON internal 03 union bcon * Embedded document interpolation type-specifier */
#define BTPD ":PD:"
/** BCON internal 04 union bcon * Array interpolation type-specifier */
#define BTPA ":PA:"

/** BCON 02 cstring string value */
#define BS(v) BTS, { .s = (v) }
/** BCON 01 double Floating point value */
#define BF(v) BTF, { .f = (v) }
/** BCON 07 cstring ObjectId value */
#define BO(v) BTO, { .o = (v) }
/** BCON 08 int Boolean value */
#define BB(v) BTB, { .b = (v) }
/** BCON 09 int64 UTC datetime value */
#define BT(v) BTT, { .t = (v) }
/** BCON 0A Null value */
#define BNULL BTN, { .v = ("") }
/** BCON 0E cstring Symbol value */
#define BX(v) BTX, { .x = (v) }
/** BCON 10 int32 32-bit Integer value */
#define BI(v) BTI, { .i = (v) }
/** BCON 12 int64 64-bit Integer value */
#define BL(v) BTL, { .l = (v) }

/** BCON 02 cstring* string interpolation value */
#define BPS(v) BTPS, { .Ps = (v) }
/** BCON 01 double* Floating point interpolation value */
#define BPF(v) BTPF, { .Pf = (v) }
/** BCON 07 cstring* ObjectId interpolation value */
#define BPO(v) BTPO, { .Po = (v) }
/** BCON 08 int* Boolean interpolation value */
#define BPB(v) BTPB, { .Pb = (v) }
/** BCON 09 int64* UTC datetime  value */
#define BPT(v) BTPT, { .Pt = (v) }
/** BCON 0E cstring* Symbol interpolation value */
#define BPX(v) BTPX, { .Px = (v) }
/** BCON 10 int32* 32-bit Integer interpolation value */
#define BPI(v) BTPI, { .Pi = (v) }
/** BCON 12 int64* 64-bit Integer interpolation value */
#define BPL(v) BTPL, { .Pl = (v) }
/** BCON 03 union bcon * Embedded document interpolation value */
#define BPD(v) BTPD, { .PD = (v) }
/** BCON 04 union bcon * Array interpolation value */
#define BPA(v) BTPA, { .PA = (v) }

/*
 * References on codes used for types
 *     http://en.wikipedia.org/wiki/Name_mangling
 *     http://www.agner.org/optimize/calling_conventions.pdf (page 25)
 */

typedef enum bcon_error_t {
    BCON_OK = 0, /**< OK return code */
    BCON_ERROR,  /**< ERROR return code */
    BCON_DOCUMENT_INCOMPLETE, /**< bcon document or nesting incomplete */
    BCON_BSON_ERROR /**< bson finish error */
} bcon_error_t;

extern char *bcon_errstr[]; /**< bcon_error_t text messages */

/**
 * Append a BCON object to a BSON object.
 *
 * @param b a BSON object
 * @param bc a BCON object
 */
MONGO_EXPORT bcon_error_t bson_append_bcon(bson *b, const bcon *bc);

/**
 * Generate a BSON object from a BCON object.
 *
 * @param b a BSON object
 * @param bc a BCON object
 */
MONGO_EXPORT bcon_error_t bson_from_bcon( bson *b, const bcon *bc );

/**
 * Print a string representation of a BCON object.
 *
 * @param bc the BCON object to print.
 */
MONGO_EXPORT void bcon_print( const bcon *bc );

#ifndef DOXYGEN_SHOULD_SKIP_THIS

MONGO_EXTERN_C_END

typedef enum bcon_token_t {
    Token_Default, Token_End, Token_Typespec,
    Token_OpenBrace, Token_CloseBrace, Token_OpenBracket, Token_CloseBracket,
    Token_EOD
} bcon_token_t;

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif

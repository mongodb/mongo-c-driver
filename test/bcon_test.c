/* bcon_test.c */

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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "bcon.h"

int verbose = 0;

int bcon_token(char *s);

void test_bcon_token() {
    assert(Token_Default == bcon_token(":_i:X"));
    assert(Token_Typespec == bcon_token(":_i:"));
    assert(Token_OpenBrace == bcon_token("{"));
    assert(Token_CloseBrace == bcon_token("}"));
    assert(Token_OpenBracket == bcon_token("["));
    assert(Token_CloseBracket == bcon_token("]"));
    assert(Token_EOD == bcon_token(0));
}

void test_bson_from_bcon(const bcon *bc, bcon_error_t bc_err, int bv_err ) {
    bcon_error_t ret;
    bson b[1];
    if ( verbose ) { putchar('\t'); bcon_print(bc); putchar('\n'); }
    ret = bson_from_bcon( b, bc );
    if (ret != bc_err) {
        printf("test_bson_from_bcon ret:%d(%s) != bc_err:%d(%s)\n", ret, bcon_errstr[ret], bc_err, bcon_errstr[bc_err]);
    }
    assert( ret == bc_err );
    assert( b->err == bv_err );
    if ( verbose )
        bson_print(b);
    bson_destroy( b );
}

void test_basic_types() {
    bcon basic_types[] = {"string", BS("a string"), "f(double)", BF(3.14159), "boolean", BB(1), "time", BT(time(0)), "null", BNULL, "symbol", BX("a symbol"), "int", BI(123), "long", BL(456789L), BEND};
    test_bson_from_bcon( basic_types, BCON_OK, BSON_VALID );
}

void test_basic_interpolation() {
    char *s = "a_string";
    double f = 3.14159;
    bson_bool_t bb = 1;
    time_t t = time(0);
    char *x = "a symbol";
    int i = 123;
    long l = 456789L;
    bcon basic_interpolation[] = {"string", BPS(&s), "f(double)", BPF(&f), "boolean", BPB(&bb), "time", BPT(&t), "symbol", BPX(&x), "int", BPI(&i), "long", BPL(&l), BEND};
    test_bson_from_bcon( basic_interpolation, BCON_OK, BSON_VALID );
}

void test_oid_and_interpolation() {
    char *oid_s = "010203040506070809101112";
    bcon oid_bc[] = { "_id", BO(""), "user_id", BO("010203040506070809101112"), "admin_id", BPO(&oid_s), BEND };
    if ( verbose ) { putchar('\t'); bcon_print( oid_bc ); putchar('\n'); }
    test_bson_from_bcon( oid_bc, BCON_OK, BSON_VALID );;
}

void test_invalid_structure() {
    bcon bc_incomplete[] = { "k0", BEND };
    test_bson_from_bcon( bc_incomplete, BCON_DOCUMENT_INCOMPLETE, BSON_VALID );
}

void test_problematic_structure() {
    bcon bc_incomplete[] = { "k0", BEND };
    test_bson_from_bcon( bc_incomplete, BCON_DOCUMENT_INCOMPLETE, BSON_VALID );
    bcon bc_bracket_brace[] = { "k0", "v0", "k1", "{", "k11", "v11", "]", "v12", "}", BEND };
    test_bson_from_bcon( bc_bracket_brace, BCON_OK, BSON_VALID ); /* key for now */
    bcon bc_brace_bracket[] = { "k0", "v0", "k1", "[", "k11", "v11", "}", "]", BEND };
    test_bson_from_bcon( bc_brace_bracket, BCON_OK, BSON_VALID ); /* key for now */
}

void test_valid_structure() {
    bcon bc_key_value[] = { "k0", "v0", BEND };
    test_bson_from_bcon( bc_key_value, BCON_OK, BSON_VALID );
    bcon bc_key_spec_value[] = { "k0", ":_s:", "v0", BEND };
    test_bson_from_bcon( bc_key_spec_value, BCON_OK, BSON_VALID );
    bcon bc_key_value_2[] = { "k0", "v0", "k1", "v1", BEND };
    test_bson_from_bcon( bc_key_value_2, BCON_OK, BSON_VALID );
    bcon bc_embedded[] = { "k0", "v0", "k1", "{", "k10", "v10", "k11", "v11", "}", "k2", "v2", BEND };
    test_bson_from_bcon( bc_embedded, BCON_OK, BSON_VALID );
    bcon bc_embedded_2[] = { "k0", "v0", "k1", "{", "k10", "v10", "k11", "{", "k110", "v110", "}", "k12", "v12", "}", "k2", "v2", BEND };
    test_bson_from_bcon( bc_embedded_2, BCON_OK, BSON_VALID );
    bcon bc_array[] = { "k0", "v0", "k1", "[", "v10", "v11", "v12", "]", "k2", "v2", BEND };
    test_bson_from_bcon( bc_array, BCON_OK, BSON_VALID );
    bcon bc_array_with_type[] = { "k0", "v0", "k1", "[", "v10", BI(123), BL(456789), "v12", "]", "k2", "v2", BEND };
    test_bson_from_bcon( bc_array_with_type, BCON_OK, BSON_VALID );
    bcon bc_array_2[] = { "k0", "v0", "k1", "[", "v10", "v11", "[", "v120", "v121", "]", "v13", "]", "k2", "v2", BEND };
    test_bson_from_bcon( bc_array_2, BCON_OK, BSON_VALID );
    bcon bc_doc_array[] = { "k0", "v0", "k1", "{", "k10", "v10", "k11", "[", "v110", "v111", "]", "k12", "v12", "}", "k2", "v2", BEND };
    test_bson_from_bcon( bc_doc_array, BCON_OK, BSON_VALID );
    bcon bc_array_doc[] = { "k0", "v0", "k1", "[", "v10", "v11", "{", "k120", "v120", "k121", "v121", "}", "v13", "]", "k2", "v2", BEND };
    test_bson_from_bcon( bc_array_doc, BCON_OK, BSON_VALID );
}

void test_high_order_interpolation() {
    bcon bc_child_doc[] = { "k10", "v10", "k11", "v11", BEND };
    bcon bc_parent_doc[] = { "k0", "v0", "k1", BPD(bc_child_doc), "k2", "v2", BEND };
    test_bson_from_bcon( bc_parent_doc, BCON_OK, BSON_VALID );
    bcon bc_child_array[] = { "k10", "v10", "k11", "v11", BEND };
    bcon bc_parent_doc_array[] = { "k0", "v0", "k1", BPA(bc_child_array), "k2", "v2", BEND };
    test_bson_from_bcon( bc_parent_doc_array, BCON_OK, BSON_VALID );
}

void test_example_hello_world() {
    bcon_error_t ret;
    bson b[1];

    /* JSON {"hello": "world"} */

    bcon hello[] = {"hello", "world", BEND};
    test_bson_from_bcon( hello, BCON_OK, BSON_VALID );

    if ( verbose )
        printf("\t--------\n");

    bson_init( b );
    bson_append_string( b, "hello", "world" );
    ret = bson_finish( b );
    if ( verbose )
        bson_print( b );
    bson_destroy( b );
}

void test_example_awesome() {
    bcon_error_t ret;
    bson b[1];

    /* JSON {"BSON": ["awesome", 5.05, 1986]} */

    bcon awesome[] = { "BSON", "[", "awesome", BF(5.05), BI(1986), "]", BEND };
    test_bson_from_bcon( awesome, BCON_OK, BSON_VALID );

    if  (verbose )
        printf("\t--------\n");

    bson_init( b );
    bson_append_start_array( b, "BSON" );
    bson_append_string( b, "0", "awesome" );
    bson_append_double( b, "1", 5.05 );
    bson_append_int( b, "2", 1986 );
    bson_append_finish_array( b );
    ret = bson_finish( b );
    if ( verbose )
        bson_print( b );
    bson_destroy( b );
}

void test_example_wikipedia_bcon(size_t iterations) {
    bcon_error_t ret;
    size_t i;
    bson b[1];
    bcon wikipedia[] = {
        "firstName", "John",
        "lastName" , "Smith",
        "age"      , BI(25),
        "address"  ,
        "{",
            "streetAddress", "21 2nd Street",
            "city"         , "New York",
            "state"        , "NY",
            "postalCode"   , "10021",
        "}",
        "phoneNumber",
        "[",
            "{",
                "type"  , "home",
                "number", "212 555-1234",
            "}",
            "{",
                "type"  , "fax",
                "number", "646 555-4567",
            "}",
        "]",
        BEND
    };
    for (i = 0; i < iterations; i++) {
        ret = bson_from_bcon( b, wikipedia );
        bson_destroy( b );
    }
    assert(ret == BCON_OK);
}

void test_example_wikipedia_bson(size_t iterations) {
    bcon_error_t ret;
    size_t i;
    bson b[1];
    for (i = 0; i < iterations; i++) {
        bson_init( b );
        bson_append_string( b, "firstName", "John" );
        bson_append_string( b, "lastName" , "Smith" );
        bson_append_int( b, "age"      , 25);
        bson_append_start_object( b, "address" );
            bson_append_string( b, "streetAddress", "21 2nd Street" );
            bson_append_string( b, "city"         , "New York" );
            bson_append_string( b, "state"        , "NY" );
            bson_append_string( b, "postalCode"   , "10021" );
        bson_append_finish_object( b );
        bson_append_start_array( b, "phoneNumber" );
            bson_append_start_object( b, "0" );
                bson_append_string( b, "type"  , "home" );
                bson_append_string( b, "number", "212 555-1234" );
            bson_append_finish_object( b );
            bson_append_start_object( b, "1" );
                bson_append_string( b, "type"  , "fax" );
                bson_append_string( b, "number", "646 555-4567" );
            bson_append_finish_object( b );
        bson_append_finish_array( b );
        ret = bson_finish( b );
        bson_destroy( b );
    }
    assert(ret == BSON_OK);
}

void test_example_wikipedia() {
    bson b[1];
    /*
    http://en.wikipedia.org/wiki/JSON
    {
        "firstName": "John",
        "lastName" : "Smith",
        "age"      : 25,
        "address"  :
        {
            "streetAddress": "21 2nd Street",
            "city"         : "New York",
            "state"        : "NY",
            "postalCode"   : "10021"
        },
        "phoneNumber":
        [
            {
                "type"  : "home",
                "number": "212 555-1234"
            },
            {
                "type"  : "fax",
                "number": "646 555-4567"
            }
        ]
    }
    */
/*
    extern char *benchmark_report_delim;
    benchmark_report_delim = "";
    benchmark(stdout, "test_example_wikipedia_bcon", test_example_wikipedia_bcon, 1, 1, 1.0, 10);
    printf("\n\t--------\n");
    benchmark_report_delim = "";
    benchmark(stdout, "test_example_wikipedia_bson", test_example_wikipedia_bson, 1, 1, 1.0, 10);
    putchar('\n');
 */
}

#define NAME_VALUE(x) { #x, x }

struct test_suite {
    char *name;
    void (*fn)();
} test_suite[] = {
    NAME_VALUE(test_bcon_token),
    NAME_VALUE(test_basic_types),
    NAME_VALUE(test_basic_interpolation),
    NAME_VALUE(test_oid_and_interpolation),
    NAME_VALUE(test_invalid_structure),
    NAME_VALUE(test_valid_structure),
    NAME_VALUE(test_problematic_structure),
    NAME_VALUE(test_high_order_interpolation),
    NAME_VALUE(test_example_hello_world),
    NAME_VALUE(test_example_awesome),
    /* NAME_VALUE(test_example_wikipedia), */
};

int main(int argc, char **argv) {
    int i;
    if (argc > 1)
        verbose = 1;
    for (i = 0; i < sizeof(test_suite)/sizeof(struct test_suite); i++) {
        if ( verbose )
            printf("%s:\n", test_suite[i].name);
        (*test_suite[i].fn)();
    }
    return 0;
}

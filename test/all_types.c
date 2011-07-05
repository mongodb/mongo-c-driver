#include "test.h"
#include "bson.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    bson_buffer bb;
    bson b;
    bson_iterator it, it2, it3;
    bson_oid_t oid;
    bson_timestamp_t ts;
    bson_timestamp_t ts_result;

    ts.i = 1;
    ts.t = 2;

    bson_buffer_init(&bb);
    bson_append_double(&bb, "d", 3.14);
    bson_append_string(&bb, "s", "hello");
    bson_append_string_n(&bb, "s_n", "goodbye cruel world", 7);

    {
        bson_append_start_object(&bb, "o");
            bson_append_start_array(&bb, "a");
                bson_append_binary(&bb, "0", 8, "w\0rld", 5);
            bson_append_finish_object(&bb);
        bson_append_finish_object(&bb);
    }

    bson_append_undefined(&bb, "u");

    bson_oid_from_string(&oid, "010203040506070809101112");
    ASSERT(!memcmp(oid.bytes, "\x001\x002\x003\x004\x005\x006\x007\x008\x009\x010\x011\x012", 12));
    bson_append_oid(&bb, "oid", &oid);

    bson_append_bool(&bb, "b", 1);
    bson_append_date(&bb, "date", 0x0102030405060708);
    bson_append_null(&bb, "n");
    bson_append_regex(&bb, "r", "^asdf", "imx");
    /* no dbref test (deprecated) */
    bson_append_code(&bb, "c", "function(){}");
    bson_append_code_n(&bb, "c_n", "function(){}garbage", 12);
    bson_append_symbol(&bb, "symbol", "SYMBOL");
    bson_append_symbol_n(&bb, "symbol_n", "SYMBOL and garbage", 6);

    {
        bson_buffer scope_buf;
        bson scope;
        bson_buffer_init(&scope_buf);
        bson_append_int(&scope_buf, "i", 123);
        bson_from_buffer(&scope, &scope_buf);

        bson_append_code_w_scope(&bb, "cws", "function(){return i}", &scope);
        bson_destroy(&scope);
    }

    bson_append_timestamp(&bb, "timestamp", &ts);
    bson_append_long(&bb, "l", 0x1122334455667788);

    bson_from_buffer(&b, &bb);

    bson_print(&b);

    bson_iterator_init(&it, b.data);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_DOUBLE);
    ASSERT(bson_iterator_type(&it) == BSON_DOUBLE);
    ASSERT(!strcmp(bson_iterator_key(&it), "d"));
    ASSERT(bson_iterator_double(&it) == 3.14);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_STRING);
    ASSERT(bson_iterator_type(&it) == BSON_STRING);
    ASSERT(!strcmp(bson_iterator_key(&it), "s"));
    ASSERT(!strcmp(bson_iterator_string(&it), "hello"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_STRING);
    ASSERT(bson_iterator_type(&it) == BSON_STRING);
    ASSERT(!strcmp(bson_iterator_key(&it), "s_n"));
    ASSERT(!strcmp(bson_iterator_string(&it), "goodbye"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_OBJECT);
    ASSERT(bson_iterator_type(&it) == BSON_OBJECT);
    ASSERT(!strcmp(bson_iterator_key(&it), "o"));
    bson_iterator_subiterator(&it, &it2);

    ASSERT(bson_iterator_more(&it2));
    ASSERT(bson_iterator_next(&it2) == BSON_ARRAY);
    ASSERT(bson_iterator_type(&it2) == BSON_ARRAY);
    ASSERT(!strcmp(bson_iterator_key(&it2), "a"));
    bson_iterator_subiterator(&it2, &it3);

    ASSERT(bson_iterator_more(&it3));
    ASSERT(bson_iterator_next(&it3) == BSON_BINDATA);
    ASSERT(bson_iterator_type(&it3) == BSON_BINDATA);
    ASSERT(!strcmp(bson_iterator_key(&it3), "0"));
    ASSERT(bson_iterator_bin_type(&it3) == 8);
    ASSERT(bson_iterator_bin_len(&it3) == 5);
    ASSERT(!memcmp(bson_iterator_bin_data(&it3), "w\0rld", 5));

    ASSERT(bson_iterator_more(&it3));
    ASSERT(bson_iterator_next(&it3) == BSON_EOO);
    ASSERT(bson_iterator_type(&it3) == BSON_EOO);
    ASSERT(!bson_iterator_more(&it3));

    ASSERT(bson_iterator_more(&it2));
    ASSERT(bson_iterator_next(&it2) == BSON_EOO);
    ASSERT(bson_iterator_type(&it2) == BSON_EOO);
    ASSERT(!bson_iterator_more(&it2));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_UNDEFINED);
    ASSERT(bson_iterator_type(&it) == BSON_UNDEFINED);
    ASSERT(!strcmp(bson_iterator_key(&it), "u"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_OID);
    ASSERT(bson_iterator_type(&it) == BSON_OID);
    ASSERT(!strcmp(bson_iterator_key(&it), "oid"));
    ASSERT(!memcmp(bson_iterator_oid(&it)->bytes, "\x001\x002\x003\x004\x005\x006\x007\x008\x009\x010\x011\x012", 12));
    ASSERT(bson_iterator_oid(&it)->ints[0] == oid.ints[0]);
    ASSERT(bson_iterator_oid(&it)->ints[1] == oid.ints[1]);
    ASSERT(bson_iterator_oid(&it)->ints[2] == oid.ints[2]);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_BOOL);
    ASSERT(bson_iterator_type(&it) == BSON_BOOL);
    ASSERT(!strcmp(bson_iterator_key(&it), "b"));
    ASSERT(bson_iterator_bool(&it) == 1);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_DATE);
    ASSERT(bson_iterator_type(&it) == BSON_DATE);
    ASSERT(!strcmp(bson_iterator_key(&it), "date"));
    ASSERT(bson_iterator_date(&it) == 0x0102030405060708);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_NULL);
    ASSERT(bson_iterator_type(&it) == BSON_NULL);
    ASSERT(!strcmp(bson_iterator_key(&it), "n"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_REGEX);
    ASSERT(bson_iterator_type(&it) == BSON_REGEX);
    ASSERT(!strcmp(bson_iterator_key(&it), "r"));
    ASSERT(!strcmp(bson_iterator_regex(&it), "^asdf"));
    ASSERT(!strcmp(bson_iterator_regex_opts(&it), "imx"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_CODE);
    ASSERT(bson_iterator_type(&it) == BSON_CODE);
    ASSERT(!strcmp(bson_iterator_key(&it), "c"));
    ASSERT(!strcmp(bson_iterator_string(&it), "function(){}"));
    ASSERT(!strcmp(bson_iterator_code(&it), "function(){}"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_CODE);
    ASSERT(bson_iterator_type(&it) == BSON_CODE);
    ASSERT(!strcmp(bson_iterator_key(&it), "c_n"));
    ASSERT(!strcmp(bson_iterator_string(&it), "function(){}"));
    ASSERT(!strcmp(bson_iterator_code(&it), "function(){}"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_SYMBOL);
    ASSERT(bson_iterator_type(&it) == BSON_SYMBOL);
    ASSERT(!strcmp(bson_iterator_key(&it), "symbol"));
    ASSERT(!strcmp(bson_iterator_string(&it), "SYMBOL"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_SYMBOL);
    ASSERT(bson_iterator_type(&it) == BSON_SYMBOL);
    ASSERT(!strcmp(bson_iterator_key(&it), "symbol_n"));
    ASSERT(!strcmp(bson_iterator_string(&it), "SYMBOL"));

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_CODEWSCOPE);
    ASSERT(bson_iterator_type(&it) == BSON_CODEWSCOPE);
    ASSERT(!strcmp(bson_iterator_key(&it), "cws"));
    ASSERT(!strcmp(bson_iterator_code(&it), "function(){return i}"));

    {
        bson scope;
        bson_iterator_code_scope(&it, &scope);
        bson_iterator_init(&it2, scope.data);

        ASSERT(bson_iterator_more(&it2));
        ASSERT(bson_iterator_next(&it2) == BSON_INT);
        ASSERT(bson_iterator_type(&it2) == BSON_INT);
        ASSERT(!strcmp(bson_iterator_key(&it2), "i"));
        ASSERT(bson_iterator_int(&it2) == 123);

        ASSERT(bson_iterator_more(&it2));
        ASSERT(bson_iterator_next(&it2) == BSON_EOO);
        ASSERT(bson_iterator_type(&it2) == BSON_EOO);
        ASSERT(!bson_iterator_more(&it2));
    }

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_TIMESTAMP);
    ASSERT(bson_iterator_type(&it) == BSON_TIMESTAMP);
    ASSERT(!strcmp(bson_iterator_key(&it), "timestamp"));
    ts_result = bson_iterator_timestamp(&it);
    ASSERT( ts_result.i == 1 );
    ASSERT( ts_result.t == 2);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_LONG);
    ASSERT(bson_iterator_type(&it) == BSON_LONG);
    ASSERT(!strcmp(bson_iterator_key(&it), "l"));
    ASSERT(bson_iterator_long(&it) == 0x1122334455667788);

    ASSERT(bson_iterator_more(&it));
    ASSERT(bson_iterator_next(&it) == BSON_EOO);
    ASSERT(bson_iterator_type(&it) == BSON_EOO);
    ASSERT(!bson_iterator_more(&it));

    bson_destroy( &b );

    return 0;
}


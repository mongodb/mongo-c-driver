/*
 * Copyright 2020-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util.h"

#include "bson-parser.h"
#include "test-conveniences.h"

mongoc_write_concern_t * bson_to_write_concern (bson_t *bson, bson_error_t* error) {
    bson_parser_t *parser = NULL;
    mongoc_write_concern_t *out = NULL;
    bool *journal;
    int64_t *w_int;
    char* w_string;
    int64_t *wtimeoutms;

    parser = bson_parser_new ();
    bson_parser_bool_optional (parser, "journal", &journal);
    bson_parser_int_optional (parser, "w", &w_int);
    bson_parser_utf8_alternate (parser, "w", &w_string);
    bson_parser_int_optional (parser, "wTimeoutMS", &wtimeoutms);

    if (!bson_parser_parse (parser, bson, error)) {
        goto done;
    }

    out = mongoc_write_concern_new ();
    if (journal) {
        mongoc_write_concern_set_journal (out, *journal);
    }

    if (w_int) {
        mongoc_write_concern_set_w (out, (int32_t) *w_int);
    }

    if (w_string) {
        BSON_ASSERT (0 == strcmp (w_string, "majority"));
        mongoc_write_concern_set_wmajority (out, -1);
    }

    if (wtimeoutms) {
        mongoc_write_concern_set_wtimeout_int64 (out, *wtimeoutms);
    }

done:
    bson_parser_destroy_with_parsed_fields (parser);
    return out;
}

mongoc_read_concern_t * bson_to_read_concern (bson_t *bson, bson_error_t* error) {
    bson_parser_t *parser = NULL;
    mongoc_read_concern_t *out = NULL;
    char *level;

    parser = bson_parser_new ();
    bson_parser_utf8_optional (parser, "level", &level);

    if (!bson_parser_parse (parser, bson, error)) {
        goto done;
    }

    out = mongoc_read_concern_new ();
    if (level) {
        mongoc_read_concern_set_level (out, level);
    }

done:
    bson_parser_destroy_with_parsed_fields (parser);
    return out;
}

/* Returns 0 on error. */
static mongoc_read_mode_t string_to_read_mode (char* str, bson_error_t* error) {
    if (0 == bson_strcasecmp ("primary", str)) {
        return MONGOC_READ_PRIMARY;
    } else if (0 == bson_strcasecmp ("primarypreferred", str)) {
        return MONGOC_READ_PRIMARY_PREFERRED;
    } else if (0 == bson_strcasecmp ("secondary", str)) {
        return MONGOC_READ_SECONDARY;
    } else if (0 == bson_strcasecmp ("secondarypreferred", str)) {
        return MONGOC_READ_SECONDARY_PREFERRED;
    } else if (0 == bson_strcasecmp ("nearest", str)) {
        return MONGOC_READ_NEAREST;
    }

    test_set_error (error, "Invalid read mode: %s", str);
    return 0;
}

mongoc_read_prefs_t * bson_to_read_prefs (bson_t *bson, bson_error_t* error) {
    bson_parser_t *parser = NULL;
    mongoc_read_prefs_t *out = NULL;
    char *mode_string;
    mongoc_read_mode_t read_mode;
    bson_t *tag_sets;
    int64_t *max_staleness_seconds;
    bson_t *hedge;

    parser = bson_parser_new ();
    bson_parser_utf8 (parser, "mode", &mode_string);
    bson_parser_array_optional (parser, "tagSets", &tag_sets);
    bson_parser_int_optional (parser, "maxStalenessSeconds", &max_staleness_seconds);
    bson_parser_doc_optional (parser, "hedge", &hedge);

    if (!bson_parser_parse (parser, bson, error)) {
        goto done;
    }

    read_mode = string_to_read_mode (mode_string, error);
    if (read_mode == 0) {
        goto done;
    }

    out = mongoc_read_prefs_new (read_mode);
    if (tag_sets) {
        mongoc_read_prefs_set_tags (out, tag_sets);
    }

    if (max_staleness_seconds) {
        mongoc_read_prefs_set_max_staleness_seconds (out, *max_staleness_seconds);
    }
    
    if (hedge) {
        mongoc_read_prefs_set_hedge (out, hedge);
    }

done:
    bson_parser_destroy_with_parsed_fields (parser);
    return out;
}


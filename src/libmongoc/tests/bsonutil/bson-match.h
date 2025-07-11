/*
 * Copyright 2009-present MongoDB, Inc.
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

#ifndef BSONUTIL_BSON_MATCH_H
#define BSONUTIL_BSON_MATCH_H

#include <bsonutil/bson-val.h>

/* Matches bson values in accordance with the unified test format's "Evaluating
 * Matches" rules. */
bool
bson_match (const bson_val_t *expected, const bson_val_t *actual, bool array_of_root_docs, bson_error_t *error);

/* A bson_matcher_t may be used to extend the default matching behavior. */
typedef struct _bson_matcher_t bson_matcher_t;

bson_matcher_t *
bson_matcher_new (void);

/* Current state of an ongoing match operation, of interest to custom operators.
 * Each recursion level has its own instance on the stack. */
typedef struct bson_matcher_context_t {
   bson_matcher_t *matcher;
   const char *path;
   bool is_root;
   bool array_of_root_docs;
} bson_matcher_context_t;

typedef bool (*special_fn) (const bson_matcher_context_t *context,
                            const bson_t *assertion,
                            const bson_val_t *actual,
                            void *user_data,
                            bson_error_t *error);


/* Adds a handler function for matching a special $$ operator.
 *
 * Example:
 * bson_matcher_add_special (matcher, "$$custom", custom_matcher, NULL);
 * This would call custom_matcher whenever a "$$custom" key is encountered in an
 * expectation.
 */
void
bson_matcher_add_special (bson_matcher_t *matcher, const char *keyword, special_fn special, void *user_data);

bool
bson_matcher_match (const bson_matcher_context_t *context,
                    const bson_val_t *expected,
                    const bson_val_t *actual,
                    bson_error_t *error);

void
bson_matcher_destroy (bson_matcher_t *matcher);

#endif /* BSONUTIL_BSON_MATCH_H */

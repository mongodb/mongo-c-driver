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

#ifndef BSONUTIL_BSON_MATCH_H
#define BSONUTIL_BSON_MATCH_H

#include "bsonutil/bson-val.h"

/* Matches bson values in accordance with the unified test format's "Evaluating
 * Matches" rules. */
bool
bson_match (bson_val_t *expected, bson_val_t *actual, bson_error_t *error);

/* A bson_matcher_t may be used to extend the default matching behavior. */
typedef struct _bson_matcher_t bson_matcher_t;

bson_matcher_t *
bson_matcher_new ();

typedef bool (*special_fn) (bson_matcher_t *matcher,
                            bson_t *assertion,
                            bson_val_t *actual,
                            void *ctx,
                            char *path,
                            bson_error_t *error);

/* Adds a handler function for matching a special $$ operator.
 *
 * Example:
 * bson_matcher_add_special (matcher, "$$custom", custom_matcher, NULL);
 * This would call custom_matcher whenever a "$$custom" key is encountered in an
 * expectation.
 */
void
bson_matcher_add_special (bson_matcher_t *matcher,
                          char *keyword,
                          special_fn special,
                          void *ctx);

bool
bson_matcher_match (bson_matcher_t *matcher,
                    bson_val_t *expected,
                    bson_val_t *actual,
                    char *path,
                    bson_error_t *error);

void
bson_matcher_destroy (bson_matcher_t *matcher);

#endif /* UNIFIED_BSON_MATCH_H */

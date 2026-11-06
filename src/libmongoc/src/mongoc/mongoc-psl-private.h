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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_PSL_PRIVATE_H
#define MONGOC_PSL_PRIVATE_H

#include <bson/bson.h>

#include <stdbool.h>

// The Public Suffix List, vendored from the MongoDB specifications repository
// (see src/public-suffix-list/). Used as a deny-list safeguard when validating
// the srvAllowedHostsSuffix URI option (DRIVERS-3329).

typedef struct _mongoc_psl mongoc_psl_t;

// Load and parse the embedded PSL rules. Returns NULL and sets `error` on failure.
mongoc_psl_t *
mongoc_psl_new(bson_error_t *error);

void
mongoc_psl_destroy(mongoc_psl_t *psl);

// Lazily-initialized singleton wrapping the embedded rule list.
// Returns NULL if the embedded list failed to parse (internal error).
mongoc_psl_t *
mongoc_psl_get_singleton(void);

// Returns true if `domain` is a public suffix per the PSL algorithm.
// `domain` MUST be canonical: lowercase, Punycode (A-label), no leading or
// trailing dots, and no empty labels.
bool
mongoc_psl_is_public_suffix(const mongoc_psl_t *psl, const char *domain);

#endif // MONGOC_PSL_PRIVATE_H

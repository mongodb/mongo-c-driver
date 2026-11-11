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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The Public Suffix List, vendored from the MongoDB specifications repository
// (see src/public-suffix-list/). Used as a deny-list safeguard when validating
// the srvAllowedHostsSuffix URI option (DRIVERS-3329).

// One rule of the list, located in `mongoc_psl_label_data`.
typedef struct {
   uint32_t labels_offset; // Offset in mongoc_psl_label_data of the rule's first label.
   uint8_t nlabels;        // Number of labels. Labels are stored right-to-left, each NUL-terminated.
   uint8_t exception;      // Non-zero if the rule was prefixed with '!'.
} mongoc_psl_rule_t;

// Defined in the generated mongoc-psl-data.c.
extern const char mongoc_psl_label_data[];
extern const mongoc_psl_rule_t mongoc_psl_rules[];
extern const size_t mongoc_psl_rules_count;

// Returns true if `domain` is a public suffix per the PSL algorithm.
// `domain` MUST be canonical: lowercase, Punycode (A-label), no leading or
// trailing dots, and no empty labels.
bool
mongoc_psl_is_public_suffix(const char *domain);

#endif // MONGOC_PSL_PRIVATE_H

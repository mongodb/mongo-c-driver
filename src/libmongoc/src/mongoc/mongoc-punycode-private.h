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

#ifndef MONGOC_PUNYCODE_PRIVATE_H
#define MONGOC_PUNYCODE_PRIVATE_H

#include <mlib/str.h>

#include <stdbool.h>
#include <stddef.h>

// Encode one DNS label (UTF-8, no dot separators) as Punycode (RFC 3492),
// without the "xn--" prefix. The input label MUST have already been case-folded
// and validated as UTF-8 by the caller (IDNA processing is done there).
//
// Returns true and appends the Punycode form to `dst` (an mstr) on success.
// Returns false if the label is not valid UTF-8 or exceeds INT32_MAX code points.
bool
mongoc_punycode_encode_label(const char *label, size_t label_len, mstr *dst);

// Decode one Punycode-encoded label (the part after "xn--") back to UTF-8.
// Returns true and appends the UTF-8 form to `dst` on success.
bool
mongoc_punycode_decode_label(const char *input, size_t input_len, mstr *dst);

// Canonicalize a dot-separated domain per the Initial DNS Seedlist Discovery
// Spec: ASCII-lowercase each label, and convert non-ASCII labels to their
// Punycode A-label form ("xn--..." prefix). Empty labels (leading, trailing,
// or doubled dots) are rejected.
//
// Returns true and appends the canonical form to `dst` on success.
bool
mongoc_punycode_canonicalize_domain(const char *domain, size_t domain_len, mstr *dst);

#endif // MONGOC_PUNYCODE_PRIVATE_H

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

#include <bson/bson-prelude.h>


#ifndef BSON_STRING_PRIVATE_H
#define BSON_STRING_PRIVATE_H

BSON_BEGIN_DECLS

bson_string_t *
_bson_string_alloc (const size_t size);

void
_bson_string_append_ex (bson_string_t *string, const char *str, const size_t len);

BSON_END_DECLS

#endif /* BSON_STRING_PRIVATE_H */

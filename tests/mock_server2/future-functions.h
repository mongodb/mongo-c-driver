/*
 * Copyright 2015 MongoDB, Inc.
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

#ifndef FUTURE_FUNCTIONS_H
#define FUTURE_FUNCTIONS_H

#include "future-value.h"
#include "future.h"
#include "macro-vargs-magic.h"
#include "mongoc-bulk-operation.h"

#undef FUTURE_PARAM
#undef PARAM_DECL
#undef LAST_PARAM_DECL
#undef FUTURE_FUNCTION

#define FUTURE_PARAM(TYPE, NAME) TYPE NAME
#define PARAM_DECL(decl) decl,
#define LAST_PARAM_DECL(decl) decl

/* declare functions like :
 *    future_t *future_cursor_next(mongoc_cursor_t *cursor, bson_t *doc);
 */

#define FUTURE_FUNCTION(RET_TYPE, FUTURE_FN, FN, ...) \
   future_t * \
   FUTURE_FN ( \
      FOREACH_EXCEPT_LAST(PARAM_DECL, __VA_ARGS__) \
      LAST_PARAM_DECL(LAST_ARG(__VA_ARGS__)) \
   );

#include "future-functions.def"

#undef FUTURE_PARAM
#undef PARAM_DECL
#undef LAST_PARAM_DECL
#undef FUTURE_FUNCTION

#endif //FUTURE_FUNCTIONS_H

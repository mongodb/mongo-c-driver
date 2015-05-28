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

/*
 * Define two sets of functions. A function in the first set, like
 * background_mongoc_cursor_next, runs a driver operation on a background
 * thread. One in the second set, like future_mongoc_cursor_next, launches
 * the background operation and returns a future_t that will resolve when
 * the operation finishes.
 *
 * These are used with mock_server2_t so you can run the driver on a thread
 * while controlling the server from the main thread.
 */

#include "future-functions.h"

#undef TYPE_
#undef TYPE
#undef NAME_
#undef NAME

/* in future-functions.def we define params like "FUTURE_PARAM(type, name)",
 * these macros unpack type and name from the pair */
#define TYPE_(X, Y) X
#define TYPE(PAIR) TYPE_ PAIR
#define NAME_(X, Y) Y
#define NAME(PAIR) NAME_ PAIR

/* define functions like:
 *
 *    static void *
 *    background_mongoc_cursor_next (void *data)
 *    {
 *       future_t *future = (future_t *) data;
 *       future_t *copy = future_new_copy (future);
 *       future_value_t return_value;
 *
 *       future_value_set_bool (
 *           &return_value,
 *           mongoc_cursor_next (
 *              future_value_get_mongoc_cursor_ptr (&future->argv[0]),
 *              future_value_get_bson_ptr_ptr (&future->argv[2 - 1]));
 *
 *       future_destroy (copy);
 *       future_resolve (future, return_value);
 *
 *       return NULL;
 *    }
 *
 * The future is copied so we can unlock it while calling mongoc_cursor_next.
 */

#undef FUTURE_PARAM
#undef FUTURE_GET
#undef FUTURE_GET_LAST
#undef FUTURE_FUNCTION

#define FUTURE_PARAM(TYPE, NAME) (TYPE, NAME)
#define FUTURE_GET_LAST(PARAM, i) XPASTE(future_value_get_, TYPE(PARAM)) (&future->argv[i])
/* add comma */
#define FUTURE_GET(PARAM, i) FUTURE_GET_LAST(PARAM, i),

#define FUTURE_FUNCTION(RET_TYPE, FUTURE_FN, FN, ...) \
   static void * \
   background_ ## FN (void *data) \
   { \
      future_t *future = (future_t *) data; \
      future_t *copy = future_new_copy (future); \
      future_value_t return_value; \
      \
      future_value_set_ ## RET_TYPE ( \
         &return_value, \
         FN ( \
            /* avoid trailing comma */ \
            FOREACH_EXCEPT_LAST(FUTURE_GET, __VA_ARGS__) \
            FUTURE_GET_LAST(LAST_ARG(__VA_ARGS__), ARGC(__VA_ARGS__) - 1) \
      )); \
      \
      future_destroy (copy); \
      future_resolve (future, return_value); \
      \
      return NULL; \
   }


#include "future-functions.def"

#undef FUTURE_PARAM
#undef FUTURE_GET
#undef FUTURE_GET_LAST
#undef FUTURE_FUNCTION

#undef PARAM_DECL
#undef LAST_PARAM_DECL
#undef FUTURE_SET
#undef FUTURE_FUNCTION

#define FUTURE_PARAM(TYPE, NAME) (TYPE, NAME)
#define LAST_PARAM_DECL(PARAM) TYPE(PARAM) NAME(PARAM)
/* add comma */
#define PARAM_DECL(PARAM, i) LAST_PARAM_DECL(PARAM),
#define FUTURE_SET(PARAM, i) XPASTE(future_value_set_, TYPE(PARAM)) (&future->argv[i], NAME(PARAM));

/* define functions like :
 *    future_t *
 *    future_cursor_next (mongoc_cursor_t *cursor,
 *                        bson_t **doc)
 *    {
 *       future_t *future = future_new (future_value_bool_type, 2);
 *       future_value_set_mongoc_cursor_ptr (&future->argv[0], cursor);
 *       future_value_set_bson_ptr_ptr (&future->argv[1], doc);
 *       future_start (future, background_mongoc_cursor_next);
 *       return future;
 *    }
 */

#define FUTURE_FUNCTION(RET_TYPE, FUTURE_FN, FN, ...) \
   future_t * \
   FUTURE_FN ( \
      /* avoid trailing comma */ \
      FOREACH_EXCEPT_LAST(PARAM_DECL, __VA_ARGS__) \
      LAST_PARAM_DECL(LAST_ARG(__VA_ARGS__)) \
   ) \
   { \
      future_t *future = future_new (future_value_ ## RET_TYPE ## _type, \
                                     ARGC(__VA_ARGS__)); \
      FOREACH(FUTURE_SET, __VA_ARGS__) \
      future_start (future, background_ ## FN); \
      return future; \
   }


#include "future-functions.def"

#undef FUTURE_PARAM
#undef PARAM_DECL
#undef LAST_PARAM_DECL
#undef FUTURE_SET
#undef FUTURE_FUNCTION

#undef TYPE_
#undef TYPE
#undef NAME_
#undef NAME

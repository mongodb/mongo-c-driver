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

#ifndef TEST_CONVENIENCES_H
#define TEST_CONVENIENCES_H

#include <bson.h>
#include <mongoc.h>

char *single_quotes_to_double (const char *str);

bool match_json (const bson_t *doc,
                 const char   *json_pattern,
                 bool          is_command,
                 const char   *filename,
                 int           lineno,
                 const char   *funcname);

#define ASSERT_MATCH(doc, json_pattern) \
   do { \
      assert (match_json (doc, json_pattern, false, \
                          __FILE__, __LINE__, __FUNCTION__)); \
   } while (0)

#define ASSERT_MATCH_COMMAND(doc, json_pattern) \
   do { \
      assert (match_json (doc, json_pattern, true, \
                          __FILE__, __LINE__, __FUNCTION__)); \
   } while (0)

#endif /* TEST_CONVENIENCES_H */

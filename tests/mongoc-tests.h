/*
 * Copyright 2013 MongoDB, Inc.
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


#ifndef MONGOC_TESTS_H
#define MONGOC_TESTS_H


#include <assert.h>
#include <bson.h>
#include <stdio.h>
#include <time.h>


BSON_BEGIN_DECLS


#define assert_cmpstr(a, b)                                             \
   do {                                                                 \
      if (((a) != (b)) && !!strcmp((a), (b))) {                         \
         fprintf(stderr, "FAIL\n\nAssert Failure: \"%s\" != \"%s\"\n",  \
                         a, b);                                         \
         abort();                                                       \
      }                                                                 \
   } while (0)


#define assert_cmpint(a, eq, b)                                         \
   do {                                                                 \
      if (!((a) eq (b))) {                                              \
         fprintf(stderr, "FAIL\n\nAssert Failure: %d %s %d\n"           \
                         "%s:%d  %s()\n",                               \
                         a, #eq, b,                                     \
                         __FILE__, __LINE__, __FUNCTION__);             \
         abort();                                                       \
      }                                                                 \
   } while (0)


static char *TEST_RESULT;


static void
run_test (const char *name,
          void (*func) (void))
{
   struct timeval begin;
   struct timeval end;
   struct timeval diff;
   bson_int64_t usec;
   double format;

   TEST_RESULT = "PASS";

   fprintf(stdout, "%-42s : ", name);
   fflush(stdout);
   gettimeofday(&begin, NULL);
   func();
   gettimeofday(&end, NULL);
   fprintf(stdout, TEST_RESULT);

   diff.tv_sec = end.tv_sec - begin.tv_sec;
   diff.tv_usec = usec = end.tv_usec - begin.tv_usec;
   if (usec < 0) {
      diff.tv_sec -= 1;
      diff.tv_usec = usec + 1000000;
   }

   format = diff.tv_sec + (diff.tv_usec / 1000000.0);
   fprintf(stdout, " : %lf\n", format);
}


BSON_END_DECLS

#endif /* MONGOC_TESTS_H */

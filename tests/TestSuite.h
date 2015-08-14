/*
 * Copyright 2014 MongoDB, Inc.
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


#ifndef TEST_SUITE_H
#define TEST_SUITE_H


#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifndef BINARY_DIR
# define BINARY_DIR "tests/binary"
#endif


#ifdef ASSERT
# undef ASSERT
#endif
#define ASSERT assert


#ifdef ASSERT_OR_PRINT
# undef ASSERT_OR_PRINT
#endif
#define ASSERT_OR_PRINT(_statement, _err) \
   do { \
      if (! (_statement)) { \
         fprintf(stderr, "FAIL:%s:%d  %s()\n  %s\n  %s\n\n", \
                         __FILE__, __LINE__, __FUNCTION__, \
                         #_statement, _err.message); \
         fflush(stderr); \
         abort(); \
      } \
   } while (0)


#ifdef ASSERT_CMPINT
# undef ASSERT_CMPINT
#endif
#define ASSERT_CMPINT(a, eq, b) \
   do { \
      if (!((a) eq (b))) { \
         fprintf(stderr, "FAIL\n\nAssert Failure: %d %s %d\n" \
                         "%s:%d  %s()\n", \
                         a, #eq, b, \
                         __FILE__, __LINE__, __FUNCTION__); \
         abort(); \
      } \
   } while (0)


#ifdef ASSERT_ALMOST_EQUAL
# undef ASSERT_ALMOST_EQUAL
#endif
#define ASSERT_ALMOST_EQUAL(a, b) \
   do { \
      /* evaluate once */ \
      int64_t _a = (a); \
      int64_t _b = (b); \
      if (!(_a > (_b * 4) / 5 && (_a < (_b * 6) / 5))) { \
         fprintf(stderr, "FAIL\n\nAssert Failure: %" PRId64 \
                         " not within 20%% of %" PRId64 "\n" \
                         "%s:%d  %s()\n", \
                         _a, _b, \
                         __FILE__, __LINE__, __FUNCTION__); \
         abort(); \
      } \
   } while (0)


#define ASSERT_CMPSTR(a, b) \
   do { \
      if (((a) != (b)) && !!strcmp((a), (b))) { \
         fprintf(stderr, "FAIL\n\nAssert Failure: \"%s\" != \"%s\"\n", \
                         a, b); \
         abort(); \
      } \
   } while (0)


#define ASSERT_CONTAINS(a, b) \
   do { \
      if (NULL == strstr ((a), (b))) { \
         fprintf(stderr, \
                 "FAIL\n\nAssert Failure: \"%s\" does not contain \"%s\"\n", \
                 a, b); \
         abort(); \
      } \
   } while (0)

#define ASSERT_STARTSWITH(a, b) \
   do { \
      if ((a) != strstr ((a), (b))) { \
         fprintf(stderr, \
                 "FAIL\n\nAssert Failure: \"%s\" does not start with \"%s\"\n", \
                 a, b); \
         abort(); \
      } \
   } while (0)

#define AWAIT(_condition) \
   do { \
      int64_t _start = bson_get_monotonic_time (); \
      while (! (_condition)) { \
         if (bson_get_monotonic_time() - _start > 1000 * 1000) { \
            fprintf (stderr, \
                     "%s:%d %s(): \"%s\" still false after 1 second\n", \
                     __FILE__, __LINE__, __FUNCTION__, #_condition); \
            abort (); \
         } \
      }  \
   } while (0)

#define MAX_TEST_NAME_LENGTH 500


typedef void (*TestFunc) (void);
typedef void (*TestFuncWC) (void*);
typedef void (*TestFuncDtor) (void*);
typedef struct _Test Test;
typedef struct _TestSuite TestSuite;


struct _Test
{
   Test *next;
   char *name;
   TestFuncWC func;
   TestFuncDtor dtor;
   void *ctx;
   int exit_code;
   unsigned seed;
   int (*check) (void);
};


struct _TestSuite
{
   char *prgname;
   char *name;
   char *testname;
   Test *tests;
   FILE *outfile;
   int flags;
};


void TestSuite_Init    (TestSuite *suite,
                        const char *name,
                        int argc,
                        char **argv);
void TestSuite_Add     (TestSuite *suite,
                        const char *name,
                        TestFunc func);
void TestSuite_AddWC   (TestSuite *suite,
                        const char *name,
                        TestFuncWC func,
                        TestFuncDtor dtor,
                        void *ctx);
void TestSuite_AddFull (TestSuite *suite,
                        const char *name,
                        TestFuncWC func,
                        TestFuncDtor dtor,
                        void *ctx,
                        int (*check) (void));
int  TestSuite_Run     (TestSuite *suite);
void TestSuite_Destroy (TestSuite *suite);

#ifdef __cplusplus
}
#endif


#endif /* TEST_SUITE_H */

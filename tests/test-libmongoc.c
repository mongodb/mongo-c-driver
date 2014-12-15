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


#include <bson.h>
#include <mongoc.h>
#include <stdlib.h>
#include <stdio.h>

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mongoc-tests.h"


extern void test_array_install            (TestSuite *suite);
extern void test_buffer_install           (TestSuite *suite);
extern void test_bulk_install             (TestSuite *suite);
extern void test_client_install           (TestSuite *suite);
extern void test_client_pool_install      (TestSuite *suite);
extern void test_collection_install       (TestSuite *suite);
extern void test_cursor_install           (TestSuite *suite);
extern void test_database_install         (TestSuite *suite);
extern void test_gridfs_install           (TestSuite *suite);
extern void test_gridfs_file_page_install (TestSuite *suite);
extern void test_list_install             (TestSuite *suite);
extern void test_matcher_install          (TestSuite *suite);
extern void test_queue_install            (TestSuite *suite);
extern void test_read_prefs_install       (TestSuite *suite);
extern void test_rpc_install              (TestSuite *suite);
extern void test_socket_install           (TestSuite *suite);
extern void test_stream_install           (TestSuite *suite);
extern void test_uri_install              (TestSuite *suite);
extern void test_write_command_install    (TestSuite *suite);
extern void test_write_concern_install    (TestSuite *suite);
#ifdef MONGOC_ENABLE_SSL
extern void test_x509_install             (TestSuite *suite);
extern void test_stream_tls_install       (TestSuite *suite);
#endif


static int gSuppressCount;


void
suppress_one_message (void)
{
   gSuppressCount++;
}


static void
log_handler (mongoc_log_level_t  log_level,
             const char         *log_domain,
             const char         *message,
             void               *user_data)
{
   if (gSuppressCount) {
      gSuppressCount--;
      return;
   }
   if (log_level < MONGOC_LOG_LEVEL_INFO) {
      mongoc_log_default_handler (log_level, log_domain, message, NULL);
   }
}


char MONGOC_TEST_HOST [1024];
char MONGOC_TEST_UNIQUE [32];

char *
gen_collection_name (const char *str)
{
   return bson_strdup_printf ("%s_%u_%u",
                              str,
                              (unsigned)time(NULL),
                              (unsigned)gettestpid());

}

static void
set_mongoc_test_host(void)
{
#ifdef _MSC_VER
   size_t buflen;

   if (0 != getenv_s (&buflen, MONGOC_TEST_HOST, sizeof MONGOC_TEST_HOST, "MONGOC_TEST_HOST")) {
      bson_strncpy (MONGOC_TEST_HOST, "localhost", sizeof MONGOC_TEST_HOST);
   }
#else
   if (getenv("MONGOC_TEST_HOST")) {
      bson_strncpy (MONGOC_TEST_HOST, getenv("MONGOC_TEST_HOST"), sizeof MONGOC_TEST_HOST);
   } else {
      bson_strncpy (MONGOC_TEST_HOST, "localhost", sizeof MONGOC_TEST_HOST);
   }
#endif
}


int
main (int   argc,
      char *argv[])
{
   TestSuite suite;
   int ret;

   mongoc_init ();

   bson_snprintf (MONGOC_TEST_UNIQUE, sizeof MONGOC_TEST_UNIQUE,
                  "test_%u_%u", (unsigned)time (NULL),
                  (unsigned)gettestpid ());

   set_mongoc_test_host ();

   mongoc_log_set_handler (log_handler, NULL);

   TestSuite_Init (&suite, "", argc, argv);

   test_array_install (&suite);
   test_buffer_install (&suite);
   test_client_install (&suite);
   test_client_pool_install (&suite);
   test_write_command_install (&suite);
   test_bulk_install (&suite);
   test_collection_install (&suite);
   test_cursor_install (&suite);
   test_database_install (&suite);
   test_gridfs_install (&suite);
   test_gridfs_file_page_install (&suite);
   test_list_install (&suite);
   test_matcher_install (&suite);
   test_queue_install (&suite);
   test_read_prefs_install (&suite);
   test_rpc_install (&suite);
   test_socket_install (&suite);
   test_stream_install (&suite);
   test_uri_install (&suite);
   test_write_concern_install (&suite);
#ifdef MONGOC_ENABLE_SSL
   test_x509_install (&suite);
   test_stream_tls_install (&suite);
#endif

   ret = TestSuite_Run (&suite);

   TestSuite_Destroy (&suite);

   mongoc_cleanup();

   return ret;
}

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
static mongoc_client_t *gTestClient;
#ifdef MONGOC_ENABLE_SSL
static mongoc_ssl_opt_t gSSLOptions;
#endif


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


char MONGOC_TEST_UNIQUE [32];

char *
gen_collection_name (const char *str)
{
   return bson_strdup_printf ("%s_%u_%u",
                              str,
                              (unsigned)time(NULL),
                              (unsigned)gettestpid());

}

static char *
test_framework_getenv (const char *name)
{
#ifdef _MSC_VER
      char buf[1024];
      size_t buflen;

      if (0 != getenv_s (&buflen, buf, sizeof buf, name)) {
         bson_strncpy (buf, "localhost", sizeof buf);
         return bson_strdup (buf);
      } else {
         return NULL;
      }
#else

   if (getenv (name)) {
      return bson_strdup (getenv (name));
   } else {
      return NULL;
   }

#endif
}

/* true if the environment variable is set and not falsey */
bool
test_framework_getenv_bool (const char *name)
{
   char *value = test_framework_getenv (name);
   bool ret = false;

   /* if value is OTHER THAN no, false, or 0 */
   if (value &&
       strcasecmp (value, "no") &&
       strcasecmp (value, "false") &&
       strcmp (value, "0")) {
      ret = true;
   }

   bson_free (value);
   return ret;
}

char *
test_framework_get_host (void)
{
   char *host = test_framework_getenv ("MONGOC_TEST_HOST");

   return host ? host : bson_strdup ("localhost");
}

/* true if any SSL environment variables are set */
bool
test_framework_get_ssl (void)
{
   char *ssl_option_names[] = {
      "MONGOC_TEST_SSL_PEM_FILE",
      "MONGOC_TEST_SSL_PEM_PWD",
      "MONGOC_TEST_SSL_CA_FILE",
      "MONGOC_TEST_SSL_CA_DIR",
      "MONGOC_TEST_SSL_CRL_FILE",
      "MONGOC_TEST_SSL_WEAK_CERT_VALIDATION"
   };
   char *ssl_option_value;
   size_t i;

   for (i = 0; i < sizeof ssl_option_names / sizeof (char *); i++) {
      ssl_option_value = test_framework_getenv (ssl_option_names[i]);

      if (ssl_option_value) {
         bson_free (ssl_option_value);
         return true;
      }
   }

   return test_framework_getenv_bool ("MONGOC_TEST_SSL");
}

static bool
uri_has_options (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   if (!uri) { return false; }

   bson_iter_init (&iter, mongoc_uri_get_options (uri));
   return bson_iter_next (&iter);
}

/* uri is optional */
char *
test_framework_get_uri_str (const char *uri_str)
{
   char *host = test_framework_get_host ();
   char *test_uri_str_base = uri_str ?
                             bson_strdup (uri_str) :
                             bson_strdup_printf ("mongodb://%s/", host);

   mongoc_uri_t *uri_parsed = mongoc_uri_new (test_uri_str_base);
   char *test_uri_str;

   assert (uri_parsed);

   /* add "ssl=true" if needed */
   if (test_framework_get_ssl () && !mongoc_uri_get_ssl (uri_parsed)) {
      test_uri_str = bson_strdup_printf (
         "%s%s",
         test_uri_str_base,
         uri_has_options (uri_parsed) ? "&ssl=true" : "?ssl=true");
   } else {
      test_uri_str = bson_strdup (test_uri_str_base);
   }

   mongoc_uri_destroy (uri_parsed);
   bson_free (host);
   bson_free (test_uri_str_base);
   return test_uri_str;
}

static void
test_framework_set_ssl_opts (mongoc_client_t *client)
{
   assert (client);

   if (test_framework_get_ssl ()) {
#ifndef MONGOC_ENABLE_SSL
      fprintf (stderr,
               "SSL test config variables are specified in the environment, but"
               " SSL isn't enabled\n");
      abort ();
#else
      mongoc_client_set_ssl_opts (client, &gSSLOptions);
#endif
   }
}


/* uri is optional */
mongoc_client_t *
test_framework_client_new (const char *uri_str)
{
   char *test_uri_str = test_framework_get_uri_str (uri_str);
   mongoc_client_t *client = mongoc_client_new (test_uri_str);

   assert (client);
   test_framework_set_ssl_opts (client);

   bson_free (test_uri_str);
   assert (client);
   return client;
}

#ifdef MONGOC_ENABLE_SSL
static void
test_framework_global_ssl_opts_init (void)
{
   memcpy (&gSSLOptions, mongoc_ssl_opt_get_default (), sizeof gSSLOptions);

   gSSLOptions.pem_file = test_framework_getenv ("MONGOC_TEST_SSL_PEM_FILE");
   gSSLOptions.pem_pwd = test_framework_getenv ("MONGOC_TEST_SSL_PEM_PWD");
   gSSLOptions.ca_file = test_framework_getenv ("MONGOC_TEST_SSL_CA_FILE");
   gSSLOptions.ca_dir = test_framework_getenv ("MONGOC_TEST_SSL_CA_DIR");
   gSSLOptions.crl_file = test_framework_getenv ("MONGOC_TEST_SSL_CRL_FILE");
   gSSLOptions.weak_cert_validation = test_framework_getenv_bool (
      "MONGOC_TEST_SSL_WEAK_CERT_VALIDATION");
}

static void
test_framework_global_ssl_opts_cleanup (void)
{
   bson_free (gSSLOptions.pem_file);
   bson_free (gSSLOptions.pem_pwd);
   bson_free (gSSLOptions.ca_file);
   bson_free (gSSLOptions.ca_dir);
   bson_free (gSSLOptions.crl_file);
}
#endif

static void
test_framework_global_client_init (void)
{
   gTestClient = test_framework_client_new (NULL);
}

mongoc_client_t *
test_framework_get_global_client (void)
{
   assert (gTestClient);
   return gTestClient;
}

static void
test_framework_global_client_destroy (void)
{
   if (gTestClient) { mongoc_client_destroy (gTestClient); }
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

   mongoc_log_set_handler (log_handler, NULL);

#ifdef MONGOC_ENABLE_SSL
   test_framework_global_ssl_opts_init ();
   atexit (test_framework_global_ssl_opts_cleanup);
#endif
   test_framework_global_client_init ();
   atexit (test_framework_global_client_destroy);

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

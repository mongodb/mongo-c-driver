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

#include <mongoc.h>

#include "mongoc-log-private.h"
#include "TestSuite.h"


struct log_func_data {
   mongoc_log_level_t  log_level;
   const char         *log_domain;
   const char         *message;
};


void log_func (mongoc_log_level_t  log_level,
               const char         *log_domain,
               const char         *message,
               void               *user_data)
{
   struct log_func_data *data = (struct log_func_data *)user_data;
   
   data->log_level = log_level;
   data->log_domain = log_domain;
   data->message = message;
}


static void
test_mongoc_log_handler (void)
{
   mongoc_log_func_t old_handler;
   void *old_data;
   struct log_func_data data;

   _mongoc_log_get_handler (&old_handler, &old_data);
   mongoc_log_set_handler (log_func, &data);

#pragma push_macro("MONGOC_LOG_DOMAIN")
#define MONGOC_LOG_DOMAIN "my-custom-domain"

   MONGOC_WARNING ("warning!");

#pragma pop_macro("MONGOC_LOG_DOMAIN")

   ASSERT_CMPINT (data.log_level, ==, MONGOC_LOG_LEVEL_WARNING);
   ASSERT_CMPSTR (data.log_domain, "my-custom-domain");
   ASSERT_CMPSTR (data.message, "warning!");

   /* restore */
   mongoc_log_set_handler (old_handler, old_data);
}


static void
test_mongoc_log_null (void)
{
   mongoc_log_func_t old_handler;
   void *old_data;

   _mongoc_log_get_handler (&old_handler, &old_data);
   mongoc_log_set_handler (NULL, NULL);

   /* doesn't seg fault */
   MONGOC_ERROR ("error!");
   MONGOC_DEBUG ("debug!");

   /* restore */
   mongoc_log_set_handler (old_handler, old_data);
}


void
test_log_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Log/basic", test_mongoc_log_handler);
   TestSuite_Add (suite, "/Log/null", test_mongoc_log_null);
}

/*
 * Copyright 2020-present MongoDB, Inc.
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

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mongoc/mongoc.h"
#include "mongoc/mongoc-http-private.h"

void
test_mongoc_http (void *unused)
{
   mongoc_http_request_t req;
   mongoc_http_response_t res;
   bool r;
   bson_error_t error;

   BSON_UNUSED (unused);

   _mongoc_http_request_init (&req);
   _mongoc_http_response_init (&res);

   /* Basic GET request */
   req.method = "GET";
   req.host = "example.com";
   req.port = 80;
   r = _mongoc_http_send (&req, 10000, false, NULL, &res, &error);
   ASSERT_OR_PRINT (r, error);
   _mongoc_http_response_cleanup (&res);

   /* Basic POST request with a body. */
   req.method = "POST";
   req.body = "test";
   req.body_len = 4;
   req.port = 80;
   r = _mongoc_http_send (&req, 10000, false, NULL, &res, &error);
   ASSERT_OR_PRINT (r, error);
   _mongoc_http_response_cleanup (&res);
}

void
test_http_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/http",
                      test_mongoc_http,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_offline);
}

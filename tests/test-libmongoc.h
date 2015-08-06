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


#ifndef TEST_LIBMONGOC_H
#define TEST_LIBMONGOC_H


char *gen_collection_name (const char *prefix);
void suppress_one_message (void);
char *test_framework_getenv (const char *name);
char *test_framework_get_host (void);
uint16_t test_framework_get_port (void);
char *test_framework_get_admin_user (void);
char *test_framework_get_admin_password (void);
bool test_framework_get_ssl (void);
char *test_framework_add_user_password (const char *uri_str,
                                        const char *user,
                                        const char *password);
char *test_framework_get_uri_str_no_auth (const char *database_name);
char *test_framework_get_uri_str (const char *uri_str);
mongoc_uri_t *test_framework_get_uri (const char *uri_str);
mongoc_client_t *test_framework_client_new (const char *uri_str);
mongoc_client_pool_t *test_framework_client_pool_new (const char *uri_str);
#endif

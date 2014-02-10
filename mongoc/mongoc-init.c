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

#include <pthread.h>

#include "mongoc-config.h"
#include "mongoc-init.h"

#ifdef MONGOC_ENABLE_SSL
# include "mongoc-ssl.h"
# include "mongoc-ssl-private.h"
#endif

static void
_mongoc_do_init (void)
{
#ifdef MONGOC_ENABLE_SSL
   _mongoc_ssl_init();
#endif
}

void
mongoc_init (void)
{
   static pthread_once_t once = PTHREAD_ONCE_INIT;
   pthread_once (&once, _mongoc_do_init);
}

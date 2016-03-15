/*
 * Copyright 2016 MongoDB, Inc.
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

#include "mongoc-config.h"

#ifdef MONGOC_ENABLE_SSL

#include <bson.h>
#include "mongoc-ssl.h"
#include "mongoc-ssl-private.h"

#if defined(MONGOC_ENABLE_OPENSSL)
#  include "mongoc-openssl-private.h"
#elif defined(MONGOC_ENABLE_SECURE_TRANSPORT)
#  include "mongoc-secure-transport-private.h"
#endif

/* TODO: we could populate these from a config or something further down the
 * road for providing defaults */
#ifndef MONGOC_SSL_DEFAULT_TRUST_FILE
#define MONGOC_SSL_DEFAULT_TRUST_FILE NULL
#endif
#ifndef MONGOC_SSL_DEFAULT_TRUST_DIR
#define MONGOC_SSL_DEFAULT_TRUST_DIR NULL
#endif

static
mongoc_ssl_opt_t gMongocSslOptDefault = {
   NULL,
   NULL,
   MONGOC_SSL_DEFAULT_TRUST_FILE,
   MONGOC_SSL_DEFAULT_TRUST_DIR,
};

const mongoc_ssl_opt_t *
mongoc_ssl_opt_get_default (void)
{
   return &gMongocSslOptDefault;
}

char *
mongoc_ssl_extract_subject (const char *filename, const char *passphrase)
{
#if defined(MONGOC_ENABLE_OPENSSL)
	return _mongoc_openssl_extract_subject (filename, passphrase);
#elif defined(MONGOC_ENABLE_SECURE_TRANSPORT)
	return _mongoc_secure_transport_extract_subject (filename, passphrase);
#endif
}
#endif

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

#ifndef MONGOC_SSL_PRIVATE_H
#define MONGOC_SSL_PRIVATE_H

#ifdef MONGOC_ENABLE_SSL

#if !defined (MONGOC_I_AM_A_DRIVER) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "mongoc-ssl.h"
#include "mongoc-ssl-apple-private.h"
#include "mongoc-ssl-openssl-private.h"

BSON_BEGIN_DECLS

#ifdef MONGOC_OPENSSL
bool
_mongoc_ssl_check_cert (SSL        *ssl,
                        const char *host,
                        bool        weak_cert_validation);

SSL_CTX *
_mongoc_ssl_ctx_new (mongoc_ssl_opt_t *opt);
#endif

char    *
_mongoc_ssl_extract_subject (const char *filename);

void
_mongoc_ssl_init (void);

void
_mongoc_ssl_cleanup (void);

BSON_END_DECLS

#endif /* MONGOC_ENABLE_SSL */
#endif /* MONGOC_SSL_PRIVATE_H */

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

#include "mongoc-config.h"

#ifdef MONGOC_ENABLE_SSL

#include "mongoc-ssl-private.h"

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

/* TODO: it really doesn't make sense for these two methods to be part
   of the top-level SSL API, consider moving to just OpenSSL impl somehow */
#ifdef MONGOC_OPENSSL
/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_check_cert --
 *
 *-------------------------------------------------------------------------
 */

bool
_mongoc_ssl_check_cert (SSL        *ssl,
                        const char *host,
                        bool        weak_cert_validation)
{
   return _mongoc_ssl_check_cert_impl(ssl, host, weak_cert_validation);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_ctx_new --
 *
 *-------------------------------------------------------------------------
 */

SSL_CTX *
_mongoc_ssl_ctx_new (mongoc_ssl_opt_t *opt)
{
   return _mongoc_ssl_ctx_new_impl(opt);
}

#endif /* MONGOC_OPENSSL */

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_extract_subject --
 *
 *-------------------------------------------------------------------------
 */

char *
_mongoc_ssl_extract_subject (const char *filename)
{
   return _mongoc_ssl_extract_subject_impl(filename);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_init --
 *
 *       Initialize function for SSL. This needs to be called early-on
 *       and is NOT threadsafe for OpenSSL. Called by mongoc_init.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_ssl_init (void)
{
   _mongoc_ssl_init_impl();
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_cleanup --
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_ssl_cleanup (void)
{
   _mongoc_ssl_cleanup_impl();
}


#endif /* MONGOC_ENABLE_SSL */

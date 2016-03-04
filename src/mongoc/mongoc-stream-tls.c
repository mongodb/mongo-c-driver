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

#include <bson.h>

#include <errno.h>
#include <string.h>
#include "mongoc-stream-tls.h"
#include "mongoc-stream-tls-private.h"
#include "mongoc-stream-private.h"
#include "mongoc-log.h"
#include "mongoc-trace.h"

#if defined(MONGOC_ENABLE_OPENSSL)
# include "mongoc-stream-tls-openssl.h"
# include "mongoc-openssl-private.h"
#elif defined(MONGOC_ENABLE_SECURE_TRANSPORT)
# include "mongoc-secure-transport-private.h"
# include "mongoc-stream-tls-secure-transport.h"
#endif

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls"


/**
 * mongoc_stream_tls_do_handshake:
 *
 * force an ssl handshake
 *
 * This will happen on the first read or write otherwise
 */
bool
mongoc_stream_tls_do_handshake (mongoc_stream_t *stream,
                                int32_t          timeout_msec)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->do_handshake);

   return stream_tls->do_handshake(stream, timeout_msec);
}


/**
 * mongoc_stream_tls_should_retry:
 *
 * If the stream should be retried
 */
bool
mongoc_stream_tls_should_retry (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->should_retry);

   if (stream_tls->should_retry) {
      return stream_tls->should_retry (stream);
   }
   return false;
}


/**
 * mongoc_stream_tls_should_read:
 *
 * If the stream should read
 */
bool
mongoc_stream_tls_should_read (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->should_read);

   if (stream_tls->should_read) {
      return stream_tls->should_read (stream);
   }

   return false;
}


/**
 * mongoc_stream_tls_check_cert:
 *
 * check the cert returned by the other party
 */
bool
mongoc_stream_tls_check_cert (mongoc_stream_t *stream,
                              const char      *host)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->check_cert);

   return stream_tls->check_cert(stream, host);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_new --
 *
 *       Creates a new mongoc_stream_tls_t to communicate with a remote
 *       server using a TLS stream.
 *
 *       @base_stream should be a stream that will become owned by the
 *       resulting tls stream. It will be used for raw I/O.
 *
 *       @trust_store_dir should be a path to the SSL cert db to use for
 *       verifying trust of the remote server.
 *
 * Returns:
 *       NULL on failure, otherwise a mongoc_stream_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
mongoc_stream_tls_new (mongoc_stream_t  *base_stream,
                       mongoc_ssl_opt_t *opt,
                       int               client)
{
   BSON_ASSERT (base_stream);

#if defined(MONGOC_ENABLE_OPENSSL)
   return mongoc_stream_tls_openssl_new (base_stream, opt, client);
#elif defined(MONGOC_ENABLE_SECURE_TRANSPORT)
   return mongoc_stream_tls_secure_transport_new (base_stream, opt, client);
#else
#error "Don't know how to create TLS stream"
#endif
}

#endif

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

#include "mongoc-stream-tls.h"
#include "mongoc-log.h"

/* specific TLS implementations */
#include "mongoc-stream-apple-tls.h"
#include "mongoc-stream-openssl-tls.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls"

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_do_handshake:
 *
 * force an ssl handshake
 *
 * This will happen on the first read or write otherwise
 *
 *--------------------------------------------------------------------------
 */
bool
mongoc_stream_tls_do_handshake (mongoc_stream_t *stream,
                                int32_t          timeout_msec)
{
   // TODO: control which one we call?
   return mongoc_stream_tls_do_handshake_impl(stream, timeout_msec);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_check_cert --
 *
 *      check the cert returned by the other party
 *
 *--------------------------------------------------------------------------
 */
bool
mongoc_stream_tls_check_cert (mongoc_stream_t *stream,
                              const char      *host)
{
   return mongoc_stream_tls_check_cert_impl(stream, host);
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
   return (mongoc_stream_t *)mongoc_stream_tls_new_impl(base_stream, opt, client);
}

#endif

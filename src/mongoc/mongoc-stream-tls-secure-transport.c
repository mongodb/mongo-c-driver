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

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT

#include <bson.h>

#include "mongoc-trace.h"
#include "mongoc-log.h"
#include "mongoc-stream-tls.h"
#include "mongoc-stream-private.h"
#include "mongoc-stream-tls-secure-transport-private.h"
#include "mongoc-ssl.h"
#include "mongoc-counters-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls-secure_transport"

static void
_mongoc_stream_tls_secure_transport_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);

   bson_free (secure_transport);
   bson_free (stream);

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();
   EXIT;
}

static void
_mongoc_stream_tls_secure_transport_failed (mongoc_stream_t *stream)
{
   ENTRY;
   _mongoc_stream_tls_secure_transport_destroy (stream);
   EXIT;
}

static int
_mongoc_stream_tls_secure_transport_close (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}

static int
_mongoc_stream_tls_secure_transport_flush (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}

static ssize_t
_mongoc_stream_tls_secure_transport_writev (mongoc_stream_t *stream,
                                            mongoc_iovec_t  *iov,
                                            size_t           iovcnt,
                                            int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}

static ssize_t
_mongoc_stream_tls_secure_transport_readv (mongoc_stream_t *stream,
                                           mongoc_iovec_t  *iov,
                                           size_t           iovcnt,
                                           size_t           min_bytes,
                                           int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}
static int
_mongoc_stream_tls_secure_transport_setsockopt (mongoc_stream_t *stream,
                                                int              level,
                                                int              optname,
                                                void            *optval,
                                                socklen_t        optlen)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (mongoc_stream_setsockopt (tls->base_stream,
                                     level,
                                     optname,
                                     optval,
                                     optlen));
}
static mongoc_stream_t *
_mongoc_stream_tls_secure_transport_get_base_stream (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (tls->base_stream);
}


static bool
_mongoc_stream_tls_secure_transport_check_closed (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (mongoc_stream_check_closed (tls->base_stream));
}

bool
mongoc_stream_tls_secure_transport_do_handshake (mongoc_stream_t *stream,
                                                 int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

bool
mongoc_stream_tls_secure_transport_should_retry (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

bool
mongoc_stream_tls_secure_transport_should_read (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}


bool
mongoc_stream_tls_secure_transport_should_write (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

bool
mongoc_stream_tls_secure_transport_check_cert (mongoc_stream_t *stream,
                                               const char      *host)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

mongoc_stream_t *
mongoc_stream_tls_secure_transport_new (mongoc_stream_t  *base_stream,
                                        mongoc_ssl_opt_t *opt,
                                        int               client)
{
   mongoc_stream_tls_t *tls;
   mongoc_stream_tls_secure_transport_t *secure_transport;

   ENTRY;
   BSON_ASSERT(base_stream);
   BSON_ASSERT(opt);


   secure_transport = (mongoc_stream_tls_secure_transport_t *)bson_malloc0 (sizeof *secure_transport);

   tls = (mongoc_stream_tls_t *)bson_malloc0 (sizeof *tls);
   tls->parent.type = MONGOC_STREAM_TLS;
   tls->parent.destroy = _mongoc_stream_tls_secure_transport_destroy;
   tls->parent.failed = _mongoc_stream_tls_secure_transport_failed;
   tls->parent.close = _mongoc_stream_tls_secure_transport_close;
   tls->parent.flush = _mongoc_stream_tls_secure_transport_flush;
   tls->parent.writev = _mongoc_stream_tls_secure_transport_writev;
   tls->parent.readv = _mongoc_stream_tls_secure_transport_readv;
   tls->parent.setsockopt = _mongoc_stream_tls_secure_transport_setsockopt;
   tls->parent.get_base_stream = _mongoc_stream_tls_secure_transport_get_base_stream;
   tls->parent.check_closed = _mongoc_stream_tls_secure_transport_check_closed;
   tls->weak_cert_validation = opt->weak_cert_validation;
   tls->should_read = mongoc_stream_tls_secure_transport_should_read;
   tls->should_write = mongoc_stream_tls_secure_transport_should_write;
   tls->should_retry = mongoc_stream_tls_secure_transport_should_retry;
   tls->do_handshake = mongoc_stream_tls_secure_transport_do_handshake;
   tls->check_cert = mongoc_stream_tls_secure_transport_check_cert;
   tls->ctx = (void *)secure_transport;
   tls->timeout_msec = -1;
   tls->base_stream = base_stream;

   mongoc_counter_streams_active_inc();

   RETURN((mongoc_stream_t *)tls);
}
#endif /* MONGOC_ENABLE_SECURE_TRANSPORT */


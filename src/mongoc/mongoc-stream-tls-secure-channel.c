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

#ifdef MONGOC_ENABLE_SECURE_CHANNEL

#include <bson.h>

#include "mongoc-trace.h"
#include "mongoc-log.h"
#include "mongoc-stream-tls.h"
#include "mongoc-stream-private.h"
#include "mongoc-stream-tls-secure-channel-private.h"
#include "mongoc-secure-channel-private.h"
#include "mongoc-ssl.h"
#include "mongoc-error.h"
#include "mongoc-counters-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls-secure-channel"

static void
_mongoc_stream_tls_secure_channel_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);


   mongoc_stream_destroy (tls->base_stream);

   bson_free (secure_channel);
   bson_free (stream);

   mongoc_counter_streams_active_dec ();
   mongoc_counter_streams_disposed_inc ();
   EXIT;
}

static void
_mongoc_stream_tls_secure_channel_failed (mongoc_stream_t *stream)
{
   ENTRY;
   _mongoc_stream_tls_secure_channel_destroy (stream);
   EXIT;
}

static int
_mongoc_stream_tls_secure_channel_close (mongoc_stream_t *stream)
{
   int ret = 0;
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);

   ret = mongoc_stream_close (tls->base_stream);
   RETURN (ret);
}

static int
_mongoc_stream_tls_secure_channel_flush (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);
   RETURN (0);
}

static ssize_t
_mongoc_stream_tls_secure_channel_writev (mongoc_stream_t *stream,
                                            mongoc_iovec_t  *iov,
                                            size_t           iovcnt,
                                            int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (secure_channel);
   ENTRY;

   tls->timeout_msec = timeout_msec;
   RETURN (0);
}


static ssize_t
_mongoc_stream_tls_secure_channel_readv (mongoc_stream_t *stream,
                                         mongoc_iovec_t  *iov,
                                         size_t           iovcnt,
                                         size_t           min_bytes,
                                         int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (secure_channel);
   ENTRY;

   tls->timeout_msec = timeout_msec;
   RETURN (0);
}

static int
_mongoc_stream_tls_secure_channel_setsockopt (mongoc_stream_t *stream,
                                              int              level,
                                              int              optname,
                                              void            *optval,
                                              socklen_t        optlen)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);
   RETURN (mongoc_stream_setsockopt (tls->base_stream,
                                     level,
                                     optname,
                                     optval,
                                     optlen));
}

static mongoc_stream_t *
_mongoc_stream_tls_secure_channel_get_base_stream (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);
   RETURN (tls->base_stream);
}


static bool
_mongoc_stream_tls_secure_channel_check_closed (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);
   RETURN (mongoc_stream_check_closed (tls->base_stream));
}

bool
mongoc_stream_tls_secure_channel_handshake (mongoc_stream_t *stream,
                                            const char      *host,
                                            int             *events,
                                            bson_error_t    *error)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (secure_channel);

   /* TODO */

   if (1 /* nonblocking support */) {
      *events = POLLIN | POLLOUT;
   } else {
      *events = 0;
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "TLS handshake failed");
   }

   RETURN (false);
}

mongoc_stream_t *
mongoc_stream_tls_secure_channel_new (mongoc_stream_t  *base_stream,
                                      mongoc_ssl_opt_t *opt,
                                      int               client)
{
   mongoc_stream_tls_t *tls;
   mongoc_stream_tls_secure_channel_t *secure_channel;

   ENTRY;
   BSON_ASSERT (base_stream);
   BSON_ASSERT (opt);


   secure_channel = (mongoc_stream_tls_secure_channel_t *)bson_malloc0 (sizeof *secure_channel);

   tls = (mongoc_stream_tls_t *)bson_malloc0 (sizeof *tls);
   tls->parent.type = MONGOC_STREAM_TLS;
   tls->parent.destroy = _mongoc_stream_tls_secure_channel_destroy;
   tls->parent.failed = _mongoc_stream_tls_secure_channel_failed;
   tls->parent.close = _mongoc_stream_tls_secure_channel_close;
   tls->parent.flush = _mongoc_stream_tls_secure_channel_flush;
   tls->parent.writev = _mongoc_stream_tls_secure_channel_writev;
   tls->parent.readv = _mongoc_stream_tls_secure_channel_readv;
   tls->parent.setsockopt = _mongoc_stream_tls_secure_channel_setsockopt;
   tls->parent.get_base_stream = _mongoc_stream_tls_secure_channel_get_base_stream;
   tls->parent.check_closed = _mongoc_stream_tls_secure_channel_check_closed;
   memcpy (&tls->ssl_opts, opt, sizeof tls->ssl_opts);
   tls->handshake = mongoc_stream_tls_secure_channel_handshake;
   tls->ctx = (void *)secure_channel;
   tls->timeout_msec = -1;
   tls->base_stream = base_stream;

   secure_channel->must_have_one_member = (void *)NULL;

   mongoc_secure_channel_setup_certificate (secure_channel, opt);
   mongoc_secure_channel_setup_ca (secure_channel, opt);

   if (opt->ca_dir) {
      MONGOC_ERROR ("Setting mongoc_ssl_opt_t.ca_dir has no effect when built against"
            "Secure Transport");
   }
   if (opt->crl_file) {
      MONGOC_ERROR ("Setting mongoc_ssl_opt_t.crl_file has no effect when built against"
            "Secure Transport");
   }


   mongoc_counter_streams_active_inc ();
   RETURN ((mongoc_stream_t *)tls);
}
#endif /* MONGOC_ENABLE_SECURE_CHANNEL */


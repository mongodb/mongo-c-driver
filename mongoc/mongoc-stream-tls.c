/*
 * Copyright 2013 10gen Inc.
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


#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#include "mongoc-stream-tls.h"


#ifndef MONGOC_TLS_TRUST_STORE
#define MONGOC_TLS_TRUST_STORE "/etc/ssl/certs"
#endif


typedef struct
{
   mongoc_stream_t parent;
   SSL_CTX *ctx;
   SSL *ssl;
   BIO *bio;
} mongoc_stream_tls_t;


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_destroy --
 *
 *       Cleanup after usage of a mongoc_stream_tls_t. Free all allocated
 *       resources and ensure connections are closed.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
mongoc_stream_tls_destroy (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   int fd;

   BSON_ASSERT(tls);

   BIO_get_fd(tls->bio, &fd);
   close(fd);

   BIO_free_all(tls->bio);
   tls->bio = NULL;

   /*
    * TODO: Do these need to be freed individually?
    */
   tls->ssl = NULL;
   tls->ctx = NULL;

   bson_free(stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_close --
 *
 *       Close the underlying socket.
 *
 *       Linus dictates that you should not check the result of close()
 *       since there is a race condition with EAGAIN and a new file
 *       descriptor being opened.
 *
 * Returns:
 *       0 on success; otherwise -1.
 *
 * Side effects:
 *       The BIO fd is closed.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_tls_close (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   int fd = -1;

   BSON_ASSERT(tls);

   if (tls->bio) {
      BIO_get_fd(tls->bio, &fd);
      if (fd != -1) {
         close(fd);
      }
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_flush --
 *
 *       Flush the underlying stream.
 *
 * Returns:
 *       0 if successful; otherwise -1.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_tls_flush (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   BSON_ASSERT(tls);
   return BIO_flush(tls->bio);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_writev --
 *
 *       Write the iovec to the stream. This function will try to write
 *       all of the bytes or fail. If the number of bytes is not equal
 *       to the number requested, a failure or EOF has occurred.
 *
 * Returns:
 *       -1 on failure, otherwise the number of bytes written.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
mongoc_stream_tls_writev (mongoc_stream_t *stream,       /* IN */
                          struct iovec    *iov,          /* IN */
                          size_t           iovcnt,       /* IN */
                          bson_uint32_t    timeout_msec) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT(tls);

   return -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_readv --
 *
 *       Read from the stream into iov. This function will try to read
 *       all of the bytes or fail. If the number of bytes is not equal
 *       to the number requested, a failure or EOF has occurred.
 *
 * Returns:
 *       -1 on failure, 0 on EOF, otherwise the number of bytes read.
 *
 * Side effects:
 *       iov buffers will be written to.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
mongoc_stream_tls_readv (mongoc_stream_t *stream,       /* IN */
                         struct iovec    *iov,          /* INOUT */
                         size_t           iovcnt,       /* IN */
                         bson_uint32_t    timeout_msec) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT(tls);

   return -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_cork --
 *
 *       This function is not supported on mongoc_stream_tls_t.
 *
 * Returns:
 *       0 always.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_tls_cork (mongoc_stream_t *stream) /* IN */
{
   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_uncork --
 *
 *       The function is not supported on mongoc_stream_tls_t.
 *
 * Returns:
 *       0 always.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_tls_uncork (mongoc_stream_t *stream) /* IN */
{
   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_setsockopt --
 *
 *       Perform a setsockopt on the underlying socket.
 *
 * Returns:
 *       -1 on failure, otherwise opt specific value.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_tls_setsockopt (mongoc_stream_t *stream,  /* IN */
                              int              level,   /* IN */
                              int              optname, /* IN */
                              void            *optval,  /* IN */
                              socklen_t        optlen)  /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   int fd = -1;

   BSON_ASSERT(tls);

   if (tls->bio) {
      BIO_get_fd(tls->bio, &fd);
      if (fd != -1) {
         return setsockopt(fd, level, optname, optval, optlen);
      }
   }

   return -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_new --
 *
 *       Creates a new mongoc_stream_tls_t to communicate with a remote
 *       server using a TLS stream.
 *
 *       @hostname should be the hostname to connect to. This may be either
 *       a DNS hostname or a IPv4 style address such as "127.0.0.1".
 *
 *       @port should be the port to communicate with on the remote
 *       server. If @port is 0, then 27017 will be used.
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
mongoc_stream_tls_new (const char    *hostname,        /* IN */
                       bson_uint16_t  port,            /* IN */
                       const char    *trust_store_dir) /* IN */
{
   mongoc_stream_tls_t *tls;
   SSL_CTX *ctx;
   SSL *ssl;
   BIO *bio;

   if (!hostname) {
      hostname = "127.0.0.1";
   }

   if (!port) {
      port = 27017;
   }

   if (!trust_store_dir) {
      trust_store_dir = MONGOC_TLS_TRUST_STORE;
   }

   ctx = SSL_CTX_new(SSLv23_client_method());

   bio = BIO_new_ssl_connect(ctx);
   BIO_get_ssl(bio, &ssl);
   SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

   BIO_set_conn_hostname(bio, hostname);
   BIO_set_conn_port(bio, port);

   if (BIO_do_connect(bio) <= 0) {
      goto failure;
   }

   tls = bson_malloc0(sizeof *tls);
   tls->ctx = ctx;
   tls->bio = bio;
   tls->ssl = ssl;

   tls->parent.destroy = mongoc_stream_tls_destroy;
   tls->parent.close = mongoc_stream_tls_close;
   tls->parent.flush = mongoc_stream_tls_flush;
   tls->parent.writev = mongoc_stream_tls_writev;
   tls->parent.readv = mongoc_stream_tls_readv;
   tls->parent.cork = mongoc_stream_tls_cork;
   tls->parent.uncork = mongoc_stream_tls_uncork;
   tls->parent.setsockopt = mongoc_stream_tls_setsockopt;

   return (mongoc_stream_t *)tls;

failure:
   SSL_CTX_free(ctx);
   BIO_free_all(bio);

   return NULL;
}

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

#ifndef MONGOC_STREAM_TLS_PRIVATE_H
#define MONGOC_STREAM_TLS_PRIVATE_H

#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
# error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "mongoc-ssl.h"
#include "mongoc-stream.h"

#ifdef MONGOC_ENABLE_OPENSSL
#define MONGOC_TLS_TYPE 1
#elif defined(MONGOC_ENABLE_SECURE_TRANSPORT)
#define MONGOC_TLS_TYPE 2
#endif

BSON_BEGIN_DECLS

/* Available TLS Implementations */
typedef enum
{
   MONGOC_TLS_OPENSSL = 1,
   MONGOC_TLS_SECURE_TRANSPORT = 2
} mongoc_tls_types_t;

/**
 * mongoc_stream_tls_t:
 *
 * Overloaded mongoc_stream_t with additional TLS handshake and verification
 * callbacks.
 * 
 */
struct _mongoc_stream_tls_t
{
   mongoc_stream_t  parent;      /* The TLS stream wrapper */
   mongoc_stream_t *base_stream; /* The underlying actual stream */
   void            *ctx;         /* TLS lib specific configuration or wrappers */
   int32_t          timeout_msec;
   bool             weak_cert_validation;
   bool (*do_handshake) (mongoc_stream_t *stream, int32_t     timeout_msec);
   bool (*check_cert)   (mongoc_stream_t *stream, const char *host);
   bool (*should_retry) (mongoc_stream_t *stream);
   bool (*should_read)  (mongoc_stream_t *stream);
};


BSON_END_DECLS

#endif /* MONGOC_STREAM_TLS_PRIVATE_H */

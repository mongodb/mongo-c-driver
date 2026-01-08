/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_STREAM_PRIVATE_H
#define MONGOC_STREAM_PRIVATE_H

#include <mongoc/mongoc-stream.h> // IWYU pragma: export

//

#include <mongoc/mongoc-iovec.h>

#include <mlib/timer.h>

#include <stdint.h>


BSON_BEGIN_DECLS


#define MONGOC_STREAM_SOCKET 1
#define MONGOC_STREAM_FILE 2
#define MONGOC_STREAM_BUFFERED 3
#define MONGOC_STREAM_GRIDFS 4
#define MONGOC_STREAM_TLS 5
#define MONGOC_STREAM_GRIDFS_UPLOAD 6
#define MONGOC_STREAM_GRIDFS_DOWNLOAD 7

#define MONGOC_SOCKET_TIMEOUT_INFINITE 0
#define MONGOC_SOCKET_TIMEOUT_IMMEDIATE INT32_MIN

bool
mongoc_stream_wait(mongoc_stream_t *stream, int64_t expire_at);

mongoc_stream_t *
mongoc_stream_get_root_stream(mongoc_stream_t *stream);

bool
_mongoc_stream_writev_full(
   mongoc_stream_t *stream, mongoc_iovec_t *iov, size_t iovcnt, int64_t timeout_msec, bson_error_t *error);

/*
 * The public API for `mongoc_stream_t` uses a convention for interpreting non-positive integer timeouts that is
 * different from the convention used for socket timeouts. To reduce the incidence of errors from mixing these two
 * conventions, there are analogues of the public `mongoc_stream_t` API that expect the socket timeout convention:
 *
 * - `mongoc_stream_read_with_socket_timeout_convention`
 * - `mongoc_stream_writev_full_with_socket_timeout_convention`
 *
 * The `mongoc_stream_t` public API timeout convention:
 *   - 0: immediate timeout
 *   - <0: default timeout (MONGOC_DEFAULT_TIMEOUT_MSEC)
 *
 * The socket timeout convention:
 *   - INT32_MIN (MONGOC_SOCKET_TIMEOUT_IMMEDIATE): immediate timeout
 *   - [INT32_MIN + 1, 0]: infinite timeout
 *
 * The stream API convention is kept for backwards compatibility. The socket timeout analogues are exclusively for
 * internal use in functions that already use the socket timeout convention.
 */

ssize_t
_mongoc_stream_read_with_socket_timeout_convention(
   mongoc_stream_t *stream, void *buf, size_t count, size_t min_bytes, int32_t timeout_msec);

bool
_mongoc_stream_writev_full_with_socket_timeout_convention(
   mongoc_stream_t *stream, mongoc_iovec_t *iov, size_t iovcnt, int64_t timeout_msec, bson_error_t *error);


/**
 * The public stream API's timeout convention has no means of specifying an infinite timeout. The following "impl"
 * functions do the same thing as their similarly-named counterparts, but the timeout arguments are in POSIX convention,
 * i.e., negative values are interpreted as infinite rather than replaced with the default timeout. Custom stream
 * implementations that wrap other streams should use these functions internally in order to ensure infinite timeouts
 * are correctly propagated to underlying streams.
 */

ssize_t
_mongoc_stream_writev_impl(mongoc_stream_t *stream, mongoc_iovec_t *iov, size_t iovcnt, int32_t timeout_msec);

ssize_t
_mongoc_stream_write_impl(mongoc_stream_t *stream, void *buf, size_t count, int32_t timeout_msec);

ssize_t
_mongoc_stream_readv_impl(
   mongoc_stream_t *stream, mongoc_iovec_t *iov, size_t iovcnt, size_t min_bytes, int32_t timeout_msec);

ssize_t
_mongoc_stream_read_impl(mongoc_stream_t *stream, void *buf, size_t count, size_t min_bytes, int32_t timeout_msec);

bool
_mongoc_stream_writev_full_impl(
   mongoc_stream_t *stream, mongoc_iovec_t *iov, size_t iovcnt, int64_t timeout_msec, bson_error_t *error);


/**
 * @brief Poll the given set of streams
 *
 * @param streams Pointer to an array of stream polling parameters
 * @param nstreams The number of streams in the array
 * @param until A timer that will wake up `poll()` from blocking
 */
ssize_t
_mongoc_stream_poll_internal(mongoc_stream_poll_t *streams, size_t nstreams, mlib_timer until);

int32_t
_mongoc_stream_timeout_ms_to_posix_timeout_convention(int32_t timeout_msec);

int32_t
_mongoc_socket_timeout_ms_to_posix_timeout_convention(int32_t timeout_msec);

BSON_END_DECLS


#endif /* MONGOC_STREAM_PRIVATE_H */

/*
 * Copyright 2013 MongoDB Inc.
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


#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
#  error "Only <mongoc.h> can be included directly."
#endif

#ifndef MONGOC_SOCKET_UNIX_H
#define MONGOC_SOCKET_UNIX_H

#include "mongoc-compat.h"

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct msghdr mongoc_msghdr_t;

typedef int mongoc_fd_t;

typedef struct pollfd mongoc_pollfd_t;

static const mongoc_fd_t MONGOC_STDIN_FILENO = 0;
static const mongoc_fd_t MONGOC_FD_INVALID = -1;

#define mongoc_open open

#define mongoc_fd_is_valid(bfd) (bfd != -1)

#define mongoc_read read
#define mongoc_write write
#define mongoc_getsockopt getsockopt
#define mongoc_setsockopt setsockopt
#define mongoc_writev writev
#define mongoc_readv readv
#define mongoc_lseek lseek
#define mongoc_socket socket
#define mongoc_connect connect
#define mongoc_poll poll
#define mongoc_close close
#define mongoc_recvmsg recvmsg
#define mongoc_sendmsg sendmsg
#define mongoc_accept accept
#define mongoc_bind bind
#define mongoc_listen listen

int
mongoc_fd_set_nonblock (mongoc_fd_t fd);

#define mongoc_getsockname getsockname
#define mongoc_fstat fstat

#endif

#endif

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

#ifndef MONGOC_SOCKET_WIN32_H
#define MONGOC_SOCKET_WIN32_H

#include "mongoc-compat.h"

#ifdef _WIN32

struct iovec
{
   void  *iov_base;
   size_t iov_len;
};

typedef struct
{
   void         *msg_name;
   socklen_t     msg_namelen;
   struct iovec *msg_iov;
   size_t        msg_iovlen;
   void         *msg_control;
   size_t        msg_controllen;
   int           msg_flags;
} mongoc_msghdr_t;

typedef struct
{
   union {
      int    fd;
      SOCKET socket;
   }    u;
   bool is_socket;
} mongoc_fd_t;

typedef struct
{
   mongoc_fd_t fd;
   short       events;
   short       revents;
} mongoc_pollfd_t;

static const mongoc_fd_t MONGOC_STDIN_FILENO = { { 0 }, false };
static const mongoc_fd_t MONGOC_FD_INVALID = { { -1 }, false };

mongoc_fd_t
mongoc_open (const char *filename,
             int         flags);

bool
mongoc_fd_is_valid (mongoc_fd_t bfd);

ssize_t
mongoc_read (mongoc_fd_t bfd,
             void       *buf,
             size_t      count);

ssize_t
mongoc_write (mongoc_fd_t bfd,
              const void *buf,
              size_t      count);

int
mongoc_getsockopt (mongoc_fd_t bfd,
                   int         level,
                   int         optname,
                   void       *optval,
                   socklen_t  *optlen);

int
mongoc_setsockopt (mongoc_fd_t bfd,
                   int         level,
                   int         optname,
                   void       *optval,
                   socklen_t   optlen);

ssize_t
mongoc_writev (mongoc_fd_t   bfd,
               struct iovec *iov,
               int           iovcnt);

ssize_t
mongoc_readv (mongoc_fd_t   bfd,
              struct iovec *iov,
              int           iovcnt);

off_t
mongoc_lseek (mongoc_fd_t bfd,
              off_t       offset,
              int         whence);

mongoc_fd_t
mongoc_socket (int domain,
               int type,
               int protocol);

int
mongoc_connect (mongoc_fd_t            bfd,
                const struct sockaddr *addr,
                socklen_t              addrlen);

int
mongoc_poll (mongoc_pollfd_t *fds,
             size_t           nfds,
             int              timeout);


int
mongoc_close (mongoc_fd_t bfd);

ssize_t
mongoc_recvmsg (mongoc_fd_t      bfd,
                mongoc_msghdr_t *msg,
                int              flags);


ssize_t
mongoc_sendmsg (mongoc_fd_t      bfd,
                mongoc_msghdr_t *msg,
                int              flags);

mongoc_fd_t
mongoc_accept (mongoc_fd_t      bfd,
               struct sockaddr *addr,
               socklen_t       *addrlen);

int
mongoc_bind (mongoc_fd_t            bfd,
             const struct sockaddr *addr,
             socklen_t              addrlen);

int
mongoc_listen (mongoc_fd_t bfd,
               int         backlog);

int
mongoc_fd_set_nonblock (mongoc_fd_t fd);

int
mongoc_getsockname (mongoc_fd_t      bfd,
                    struct sockaddr *name,
                    int             *namelen);

int
mongoc_fstat (mongoc_fd_t  bfd,
              struct stat *buf);

#endif

#endif

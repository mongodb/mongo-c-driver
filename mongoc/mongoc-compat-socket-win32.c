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

#include "mongoc-compat.h"

#ifdef _WIN32

#define STACK_STRUCT_SIZE 10

#define MONGOC_WIN32_SOCKET_ONLY_WRAPPER(_fun, ...) \
   { \
      int r; \
      if (mfd.is_socket) { \
         r = _fun (__VA_ARGS__); \
         if (r == SOCKET_ERROR) { \
            _mongoc_win32_map_wsagetlasterror (); \
            return -1; \
         } \
         return r; \
      } else { \
         errno = ENOTSOCK; \
         return -1; \
      } \
   }

static void
_mongoc_win32_map_wsagetlasterror (void)
{
   int r = WSAGetLastError ();

   switch (r) {
   case WSANOTINITIALISED:
      assert (0);
      break;
   case WSAEACCES:
      errno = EACCES;
      break;
   case WSAEWOULDBLOCK:
      errno = EAGAIN;
      break;
   case WSAEINVAL:
      errno = EINVAL;
      break;
   case WSAEINTR:
      errno = EINTR;
      break;
   case WSAENOTSOCK:
      errno = ENOTSOCK;
      break;
   case WSAENOTCONN:
      errno = ENOTCONN;
      break;
   case WSAECONNRESET:
   case WSAECONNABORTED:
      errno = ECONNRESET;
      break;
   default:
      fprintf(stderr, "Unknown WSA Error: %d\n", r);
      assert (0);

      /* how do we handle these? */
      break;
   }
}

mongoc_fd_t
mongoc_open (const char *filename,
             int         flags)
{
   mongoc_fd_t mfd;
   errno_t err;

   mfd.is_socket = false;

   err = _sopen_s (&mfd.u.fd, filename, flags | _O_BINARY, _SH_DENYNO,
                   _S_IREAD | _S_IWRITE);

   if (err) {
      return MONGOC_FD_INVALID;
   }

   return mfd;
}

bool
mongoc_fd_is_valid (mongoc_fd_t mfd)
{
   if (mfd.is_socket) {
      return mfd.u.socket != INVALID_SOCKET;
   } else {
      return mfd.u.fd != -1;
   }
}

ssize_t
mongoc_read (mongoc_fd_t mfd,
             void       *buf,
             size_t      count)
{
   int r;

   if (mfd.is_socket) {
      r = recv (mfd.u.socket, buf, (int)count, 0);

      if (r == SOCKET_ERROR) {
         _mongoc_win32_map_wsagetlasterror ();
         return -1;
      } else {
         return r;
      }
   } else {
      return (ssize_t)_read (mfd.u.fd, buf, (int)count);
   }
}

ssize_t
mongoc_write (mongoc_fd_t mfd,
              const void *buf,
              size_t      count)
{
   int r;

   if (mfd.is_socket) {
      r = send (mfd.u.socket, buf, (int)count, 0);

      if (r == SOCKET_ERROR) {
         _mongoc_win32_map_wsagetlasterror ();
         return -1;
      } else {
         return r;
      }
   } else {
      return (ssize_t)_write (mfd.u.fd, buf, (int)count);
   }
}

int
mongoc_getsockopt (mongoc_fd_t mfd,
                   int         level,
                   int         optname,
                   void       *optval,
                   socklen_t  *optlen)
MONGOC_WIN32_SOCKET_ONLY_WRAPPER (getsockopt, mfd.u.socket, level, optname,
                                  (char *)optval, optlen)

int
mongoc_setsockopt (mongoc_fd_t mfd,
                   int         level,
                   int         optname,
                   void       *optval,
                   socklen_t   optlen)
MONGOC_WIN32_SOCKET_ONLY_WRAPPER (setsockopt, mfd.u.socket, level, optname,
                                  (char *)optval, optlen)

ssize_t
mongoc_writev (mongoc_fd_t   mfd,
               struct iovec *iov,
               int           iovcnt)
{
   int i;
   uint8_t *buf, *ptr;
   size_t buf_len = 0;
   ssize_t out;

   /* If we've got a socket, just head to sendmsg */
   if (mfd.is_socket) {
      mongoc_msghdr_t msghdr = { 0 };
      msghdr.msg_iov = iov;
      msghdr.msg_iovlen = iovcnt;
      return mongoc_sendmsg (mfd, &msghdr, 0);
   }

   /* with one buffer it's a regular write */
   if (iovcnt == 1) {
      return mongoc_write (mfd, iov[0].iov_base, iov[0].iov_len);
   }

   /* otherwise make a buffer to do a single write from */

   for (i = 0; i < iovcnt; i++) {
      buf_len += iov[i].iov_len;
   }

   buf = malloc (buf_len);
   ptr = buf;

   for (i = 0; i < iovcnt; i++) {
      memcpy (ptr, iov[i].iov_base, iov[i].iov_len);
      ptr += iov[i].iov_len;
   }

   out = mongoc_write (mfd, buf, buf_len);

   free (buf);

   return out;
}

ssize_t
mongoc_readv (mongoc_fd_t   mfd,
              struct iovec *iov,
              int           iovcnt)
{
   int i;
   uint8_t *buf, *ptr;
   size_t buf_len = 0, bytes_remaining;
   ssize_t total;

   /* If we've got a socket, just head to sendmsg */
   if (mfd.is_socket) {
      mongoc_msghdr_t msghdr = { 0 };
      msghdr.msg_iov = iov;
      msghdr.msg_iovlen = iovcnt;
      return mongoc_recvmsg (mfd, &msghdr, 0);
   }

   /* with one buffer it's a regular read */
   if (iovcnt == 1) {
      return mongoc_read (mfd, iov[0].iov_base, iov[0].iov_len);
   }

   /* otherwise make a buffer to do a single read into */
   for (i = 0; i < iovcnt; i++) {
      buf_len += iov[i].iov_len;
   }

   buf = malloc (buf_len);
   ptr = buf;

   total = mongoc_read (mfd, buf, buf_len);
   bytes_remaining = total;

   /* now extract back out */

   for (i = 0; i < iovcnt; i++) {
      if (bytes_remaining > iov[i].iov_len) {
         memcpy (iov[i].iov_base, ptr, iov[i].iov_len);
         ptr += iov[i].iov_len;
         bytes_remaining -= iov[i].iov_len;
      } else {
         memcpy (iov[i].iov_base, ptr, bytes_remaining);
         break;
      }
   }

   free (buf);

   return total;
}

off_t
mongoc_lseek (mongoc_fd_t mfd,
              off_t       offset,
              int         whence)
{
   if (mfd.is_socket) {
      errno = ESPIPE;
      return -1;
   }

   return _lseek (mfd.u.fd, (long)offset, whence);
}

mongoc_fd_t
mongoc_socket (int domain,
               int type,
               int protocol)
{
   mongoc_fd_t mfd;

   mfd.is_socket = true;
   mfd.u.socket = WSASocket (domain, type, protocol, NULL, 0,
                             WSA_FLAG_OVERLAPPED);

   if (mfd.u.socket == INVALID_SOCKET) {
      _mongoc_win32_map_wsagetlasterror ();
      return mfd;
   }

   return mfd;
}

int
mongoc_connect (mongoc_fd_t            mfd,
                const struct sockaddr *addr,
                socklen_t              addrlen)
MONGOC_WIN32_SOCKET_ONLY_WRAPPER (WSAConnect, mfd.u.socket, addr, addrlen, NULL,
                                  NULL, NULL, NULL)

int
mongoc_poll (mongoc_pollfd_t *fds,
             size_t           nfds,
             int              timeout)
{
   int i, j, r;
   WSAPOLLFD small_wsafds[STACK_STRUCT_SIZE];
   WSAPOLLFD *wsafds;

   if (nfds > STACK_STRUCT_SIZE) {
      wsafds = malloc (sizeof (*wsafds) * nfds);
   } else {
      wsafds = small_wsafds;
   }

   /* pull together an appropriate poll struct for WSAPoll */
   for (i = 0, j = 0; i < nfds; i++) {
      if (fds[i].fd.is_socket) {
         /* only include sockets.  fd's aren't valid */
         wsafds[j].fd = fds[i].fd.u.socket;
         wsafds[j].events = fds[i].events;
         wsafds[j].revents = fds[i].revents;
         j++;
      }
   }

   if (j) {
      r = WSAPoll (wsafds, (ULONG)j, timeout);
   } else {
      r = 0;
   }

   /* if we didn't error */
   if (r >= 0) {
      for (i = 0; i < nfds; i++) {
         if (fds[i].fd.is_socket) {
            /* sockets get real events */
            fds[i].revents = wsafds[j].revents;
            j++;
         } else {
            /* fd's are assumed to be local files, assume immediate pollin/out
             * increment poll's 'output' to compensate
             *
             * TODO: we can test std/in/out/err here, probably with actual
             * waits on handles.  That would make this not break for those
             * cases */
            r++;
            fds[i].revents = fds[i].events;
         }
      }
   }

   if (nfds > STACK_STRUCT_SIZE) {
      free (wsafds);
   }

   if (r == SOCKET_ERROR) {
      _mongoc_win32_map_wsagetlasterror ();
      return -1;
   }

   return r;
}


int
mongoc_close (mongoc_fd_t mfd)
{
   if (mfd.is_socket) {
      if (shutdown (mfd.u.socket, SD_BOTH) == SOCKET_ERROR) {
         _mongoc_win32_map_wsagetlasterror ();
         return -1;
      }

      if (closesocket (mfd.u.socket) == SOCKET_ERROR) {
         _mongoc_win32_map_wsagetlasterror ();
         return -1;
      }

      return 0;
   } else {
      return _close (mfd.u.fd);
   }
}

ssize_t
mongoc_recvmsg (mongoc_fd_t      mfd,
                mongoc_msghdr_t *msg,
                int              flags)
{
   int r;
   int i;

   WSABUF small_lpBuffers[STACK_STRUCT_SIZE];
   LPWSABUF lpBuffers;
   DWORD dwBufferCount;
   DWORD bytes_received;
   DWORD wsa_flags = 0;

   if (!mfd.is_socket) {
      errno = ENOTSOCK;
      return -1;
   }

   if (msg->msg_iovlen > STACK_STRUCT_SIZE) {
      lpBuffers = malloc (sizeof (WSABUF) * msg->msg_iovlen);
   } else {
      lpBuffers = small_lpBuffers;
   }

   for (i = 0; i < msg->msg_iovlen; i++) {
      lpBuffers[i].len = (ULONG)msg->msg_iov[i].iov_len;
      lpBuffers[i].buf = msg->msg_iov[i].iov_base;
   }

   dwBufferCount = (ULONG)msg->msg_iovlen;

   r = WSARecv (mfd.u.socket, lpBuffers, dwBufferCount, &bytes_received,
                &wsa_flags, NULL, NULL);

   if (msg->msg_iovlen > STACK_STRUCT_SIZE) {
      free (lpBuffers);
   }

   if (r == SOCKET_ERROR) {
      _mongoc_win32_map_wsagetlasterror ();
      return -1;
   }

   return bytes_received;
}


ssize_t
mongoc_sendmsg (mongoc_fd_t      mfd,
                mongoc_msghdr_t *msg,
                int              flags)
{
   int r;
   int i;

   WSABUF small_lpBuffers[STACK_STRUCT_SIZE];
   LPWSABUF lpBuffers;
   DWORD dwBufferCount;
   DWORD bytes_sent;

   if (!mfd.is_socket) {
      errno = ENOTSOCK;
      return -1;
   }

   if (msg->msg_iovlen > STACK_STRUCT_SIZE) {
      lpBuffers = malloc (sizeof (WSABUF) * msg->msg_iovlen);
   } else {
      lpBuffers = small_lpBuffers;
   }

   for (i = 0; i < msg->msg_iovlen; i++) {
      lpBuffers[i].len = (ULONG)msg->msg_iov[i].iov_len;
      lpBuffers[i].buf = msg->msg_iov[i].iov_base;
   }

   dwBufferCount = (ULONG)msg->msg_iovlen;

   r = WSASend (mfd.u.socket, lpBuffers, dwBufferCount, &bytes_sent, 0,
                NULL, NULL);

   if (msg->msg_iovlen > STACK_STRUCT_SIZE) {
      free (lpBuffers);
   }

   if (r == SOCKET_ERROR) {
      _mongoc_win32_map_wsagetlasterror ();
      return -1;
   }


   return bytes_sent;
}

mongoc_fd_t
mongoc_accept (mongoc_fd_t      mfd,
               struct sockaddr *addr,
               socklen_t       *addrlen)
{
   mongoc_fd_t out;

   out.is_socket = true;

   out.u.socket = accept (mfd.u.socket, addr, addrlen);

   if (out.u.socket == INVALID_SOCKET) {
      _mongoc_win32_map_wsagetlasterror ();
   }

   return out;
}

int
mongoc_bind (mongoc_fd_t            mfd,
             const struct sockaddr *addr,
             socklen_t              addrlen)
MONGOC_WIN32_SOCKET_ONLY_WRAPPER (bind, mfd.u.socket, addr, addrlen)

int
mongoc_listen (mongoc_fd_t mfd,
               int         backlog)
MONGOC_WIN32_SOCKET_ONLY_WRAPPER (listen, mfd.u.socket, backlog)

int
mongoc_fd_set_nonblock (mongoc_fd_t fd)
{
   return 0;
}

int
mongoc_getsockname (mongoc_fd_t      mfd,
                    struct sockaddr *name,
                    int             *namelen)
MONGOC_WIN32_SOCKET_ONLY_WRAPPER (getsockname, mfd.u.socket, name, namelen)

int
mongoc_fstat (mongoc_fd_t  mfd,
              struct stat *buf)
{
   if (mfd.is_socket) {
      errno = ESPIPE;
      return -1;
   }

   return fstat (mfd.u.fd, buf);
}

#endif

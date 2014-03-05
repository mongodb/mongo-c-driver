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


#ifndef MONGOC_COMPAT_H
#define MONGOC_COMPAT_H


#ifdef _WIN32
# if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#  undef _WIN32_WINNT
# endif
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0600
# endif
# include <winsock2.h>
# include <stdio.h>
# include <share.h>
# include <ws2tcpip.h>
# include <bson.h>
# include "mongoc-compat-socket-win32.h"
# define strcasecmp _stricmp
#else
# include <bson.h>
# include "mongoc-compat-socket-unix.h"
#endif


BSON_BEGIN_DECLS


void
_mongoc_compat_init (void);

void
_mongoc_compat_shutdown (void);


BSON_END_DECLS


#endif /* MONGOC_COMPAT_H */

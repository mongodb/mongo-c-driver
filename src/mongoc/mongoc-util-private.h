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

#ifndef MONGOC_UTIL_PRIVATE_H
#define MONGOC_UTIL_PRIVATE_H

#if !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>
#include "mongoc.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

/* string comparison functions for Windows */
#ifdef _WIN32
# define strcasecmp  _stricmp
# define strncasecmp _strnicmp
#endif

/* Suppress CWE-252 ("Unchecked return value") warnings for things we can't deal with */
#if defined(__GNUC__) && __GNUC__ >= 4
# define _ignore_value(x) (({ __typeof__ (x) __x = (x); (void) __x; }))
#else
# define _ignore_value(x) ((void) (x))
#endif

#define COALESCE(x, y) ((x == 0) ? (y) : (x))

#ifdef _WIN32
# define MONGOC_RAND_R rand_s
#else
# define MONGOC_RAND_R rand_r
#endif

/* Helper macros for stringifying things */
#define MONGOC_STR(s) #s
#define MONGOC_EVALUATE_STR(s) MONGOC_STR (s)

BSON_BEGIN_DECLS


char *_mongoc_hex_md5 (const char *input);

void _mongoc_usleep (int64_t usec);

const char *_mongoc_get_command_name (const bson_t *command);

void _mongoc_get_db_name (const char *ns,
                          char *db /* OUT */);

void _mongoc_bson_destroy_if_set (bson_t *bson);

size_t
_mongoc_strlen_or_zero (const char *s);

bool
_mongoc_get_server_id_from_opts (const bson_t          *opts,
                                 mongoc_error_domain_t  domain,
                                 mongoc_error_code_t    code,
                                 uint32_t              *server_id,
                                 bson_error_t          *error);

BSON_END_DECLS

#endif /* MONGOC_UTIL_PRIVATE_H */

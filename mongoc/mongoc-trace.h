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


#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif


#ifndef MONGOC_TRACE_H
#define MONGOC_TRACE_H


#include <bson.h>
#include <ctype.h>

#include "mongoc-log.h"


BSON_BEGIN_DECLS


#ifdef MONGOC_TRACE
#define ENTRY       do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "ENTRY: %s():%d", __FUNCTION__, __LINE__); } while (0)
#define EXIT        do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", __FUNCTION__, __LINE__); return; } while (0)
#define RETURN(ret) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", __FUNCTION__, __LINE__); return ret; } while (0)
#define GOTO(label) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " GOTO: %s():%d %s", __FUNCTION__, __LINE__, #label); goto label; } while (0)
#define DUMP_BYTES(_n, _b, _l) \
   do { \
      bson_string_t *str, *astr; \
      bson_int32_t _i; \
      bson_uint8_t _v; \
      mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                 " %s = %p [%d]", #_n, _b, (int)_l); \
      str = bson_string_new(NULL); \
      astr = bson_string_new(NULL); \
      for (_i = 0; _i < _l; _i++) { \
         _v = *(_b + _i); \
         if ((_i % 16) == 0) { \
            bson_string_append_printf(str, "%06x: ", _i); \
         } \
         bson_string_append_printf(str, " %02x", _v); \
         if (isprint(_v)) { \
            bson_string_append_printf(astr, " %c", _v); \
         } else { \
            bson_string_append(astr, " ."); \
         } \
         if ((_i % 16) == 15) { \
            mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                       "%s %s", str->str, astr->str); \
            bson_string_truncate(str, 0); \
            bson_string_truncate(astr, 0); \
         } else if ((_i % 16) == 7) { \
            bson_string_append(str, " "); \
            bson_string_append(astr, " "); \
         } \
      } \
      if (_i != 16) { \
         mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                    "%-56s %s", str->str, astr->str); \
      } \
      bson_string_free(str, TRUE); \
      bson_string_free(astr, TRUE); \
   } while (0)
#else
#define ENTRY
#define EXIT        return
#define RETURN(ret) return ret
#define GOTO(label) goto label
#define DUMP_BYTES(_n, _b, _l)
#endif


BSON_END_DECLS


#endif /* MONGOC_TRACE_H */

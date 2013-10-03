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


#ifndef MONGOC_TRACE_H
#define MONGOC_TRACE_H


#include <bson.h>

#include "mongoc-log.h"


BSON_BEGIN_DECLS


#ifdef MONGOC_TRACE
#define ENTRY       do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "ENTRY: %s():%d", __FUNCTION__, __LINE__); } while (0)
#define EXIT        do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", __FUNCTION__, __LINE__); return; } while (0)
#define RETURN(ret) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", __FUNCTION__, __LINE__); return ret; } while (0)
#define GOTO(label) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " GOTO: %s():%d %s", __FUNCTION__, __LINE__, #label); goto label; } while (0)
#else
#define ENTRY
#define EXIT        return
#define RETURN(ret) return ret
#define GOTO(label) goto label
#endif


BSON_END_DECLS


#endif /* MONGOC_TRACE_H */

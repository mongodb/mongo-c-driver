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


#ifndef MONGOC_H
#define MONGOC_H


#include <bson.h>

#define MONGOC_INSIDE
#include "mongoc-build.h"
#include "mongoc-client.h"
#include "mongoc-client-pool.h"
#include "mongoc-collection.h"
#include "mongoc-cursor.h"
#include "mongoc-database.h"
#include "mongoc-error.h"
#include "mongoc-flags.h"
#include "mongoc-host-list.h"
#include "mongoc-opcode.h"
#include "mongoc-log.h"
#include "mongoc-stream.h"
#include "mongoc-stream-buffered.h"
#include "mongoc-stream-unix.h"
#include "mongoc-stdint.h"
#include "mongoc-uri.h"
#include "mongoc-write-concern.h"
#include "mongoc-version.h"

#ifdef MONGOC_HAVE_SSL
#include "mongoc-stream-tls.h"
#include "mongoc-ssl.h"
#endif

#undef MONGOC_INSIDE


#endif /* MONGOC_H */

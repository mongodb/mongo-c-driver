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


#ifndef MONGOC_HANDSHAKE_H
#define MONGOC_HANDSHAKE_H

#include <mongoc/mongoc-macros.h>

#include <bson/bson.h>

BSON_BEGIN_DECLS

#define MONGOC_HANDSHAKE_APPNAME_MAX 128

MONGOC_EXPORT (bool)
mongoc_handshake_data_append (const char *driver_name, const char *driver_version, const char *platform);

BSON_END_DECLS

#endif

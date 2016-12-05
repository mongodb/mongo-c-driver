/*
 * Copyright 2016 MongoDB, Inc.
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


#ifndef MONGOC_METADATA_PRIVATE_H
#define MONGOC_METADATA_PRIVATE_H

#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
# error "Only <mongoc.h> can be included directly."
#endif
#include <bson.h>

BSON_BEGIN_DECLS

#define METADATA_FIELD "client"
#define METADATA_PLATFORM_FIELD "platform"

#define METADATA_MAX_SIZE 512

#define METADATA_OS_TYPE_MAX 32
#define METADATA_OS_NAME_MAX 32
#define METADATA_OS_VERSION_MAX 32
#define METADATA_OS_ARCHITECTURE_MAX 32
#define METADATA_DRIVER_NAME_MAX 64
#define METADATA_DRIVER_VERSION_MAX 32
/* platform has no fixed max size. It can just occupy the remaining
 * available space in the document. */

typedef struct _mongoc_metadata_t
{
   char *os_type;
   char *os_name;
   char *os_version;
   char *os_architecture;

   char *driver_name;
   char *driver_version;
   char *platform;

   bool  frozen;
} mongoc_metadata_t;

void               _mongoc_metadata_init                       (void);
void               _mongoc_metadata_cleanup                    (void);

bool               _mongoc_metadata_build_doc_with_application (bson_t     *doc,
                                                                const char *application);
void               _mongoc_metadata_freeze                     (void);
mongoc_metadata_t *_mongoc_metadata_get                        (void);
BSON_END_DECLS

#endif

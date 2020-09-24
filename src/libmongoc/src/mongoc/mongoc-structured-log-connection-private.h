/*
 * Copyright 2020 MongoDB, Inc.
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

#include "mongoc-prelude.h"

#ifndef MONGOC_STRUCTURED_LOG_CONNECTION_PRIVATE_H
#define MONGOC_STRUCTURED_LOG_CONNECTION_PRIVATE_H

#include "mongoc-structured-log.h"
#include "mongoc-cmd-private.h"

void
mongoc_structured_log_connection_client_created (void);

#endif /* MONGOC_STRUCTURED_LOG_COMMAND_PRIVATE_H */

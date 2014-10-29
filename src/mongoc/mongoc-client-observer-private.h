/*
 * Copyright 2014 MongoDB, Inc.
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

#ifndef MONGOC_CLIENT_OBSERVER_PRIVATE_H
#define MONGOC_CLIENT_OBSERVER_PRIVATE_H

#include "mongoc-client-observer.h"

/*
 *--------------------------------------------------------------------------
 *
 * The following are trigger functions for various callbacks defined in the
 * observer table. These trigger functions are meant to be called
 * internally only.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_client_observer_trigger_command     (mongoc_client_observer_t *table,
                                            const bson_t *command,
                                            char ns[]);

void
mongoc_client_observer_trigger_socket_bind (mongoc_client_observer_t *table,
                                            mongoc_socket_t *sock,
                                            const struct sockaddr *addr);

#endif /* MONGOC_CLIENT_OBSERVER_PRIVATE_H */

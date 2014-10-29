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

#ifndef MONGOC_CLIENT_OBSERVER_H
#define MONGOC_CLIENT_OBSERVER_H

#include "mongoc-socket.h"

#define MONGOC_CLIENT_OBSERVER_SIZE 16

/*
 *--------------------------------------------------------------------------
 *
 * These are the supported observable events:
 *
 *--------------------------------------------------------------------------
 */
typedef enum
{
    MONGOC_CLIENT_OBSERVER_COMMAND,
    MONGOC_CLIENT_OBSERVER_SOCKET_BIND,
} mongoc_client_observer_event_name_t;

/*
 *--------------------------------------------------------------------------
 *
 * The supported callbacks' signatures:
 *
 *--------------------------------------------------------------------------
 */
typedef void (*mongoc_client_observer_callback_t)      (void);
typedef void (*mongoc_client_observer_command_t)       (const bson_t *command,
                                                        char ns[],
                                                        void *user_data);
typedef void (*mongoc_client_observer_socket_bind_t)   (mongoc_socket_t *sock,
                                                        const struct sockaddr *adr,
                                                        void *user_data);

/*
 *--------------------------------------------------------------------------
 * mongoc_client_observer_function_t --
 *
 *      A callback function and its name.
 *
 *--------------------------------------------------------------------------
 */
typedef struct _mongoc_client_observer_function {
    mongoc_client_observer_event_name_t name;
    mongoc_client_observer_callback_t callback;
} mongoc_client_observer_function_t;

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_observer_t --
 *
 *      The mongoc_client_observer_t keeps a directory of callback functions
 *      to be triggered on certain key events.  It also contains some
 *      arbitrary user data, and a destructor for that data.
 *
 *--------------------------------------------------------------------------
 */
typedef struct _mongoc_client_observer_t {
    mongoc_client_observer_callback_t callbacks[MONGOC_CLIENT_OBSERVER_SIZE];
    void *user_data;
} mongoc_client_observer_t;


/*
 *--------------------------------------------------------------------------
 *
 * Initialize a new observer table with 'num_callbacks' callback functions.
 * 'user_data' must outlive this table.
 *
 *--------------------------------------------------------------------------
 */
mongoc_client_observer_t
*mongoc_client_observer_new(mongoc_client_observer_function_t *callbacks,
                            int num_callbacks,
                            void *user_data);

/*
 *--------------------------------------------------------------------------
 *
 * destroy this observer table and its associated data.
 *
 *--------------------------------------------------------------------------
 */
void mongoc_client_observer_destroy(mongoc_client_observer_t *table);

#endif /* MONGOC_CLIENT_OBSERVER_H */

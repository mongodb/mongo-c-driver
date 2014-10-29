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

#include "mongoc-client-observer-private.h"

mongoc_client_observer_t
*mongoc_client_observer_new(mongoc_client_observer_function_t *callbacks,
                            int num_callbacks,
                            void *user_data)
{
    mongoc_client_observer_t *observer;

    observer = bson_malloc0(sizeof *observer);
    observer->user_data = user_data;

    /* set all the callbacks to null */
    for (int i = 0; i < MONGOC_CLIENT_OBSERVER_SIZE; i++) {
        observer->callbacks[i] = NULL;
    }

    /* sort the custom callbacks into their respective rows in table */
    for (int i = 0; i < num_callbacks; i++) {
        mongoc_client_observer_function_t entry = callbacks[i];
        observer->callbacks[entry.name] = entry.callback;
    }
    return observer;
}

void mongoc_client_observer_destroy(mongoc_client_observer_t *observer) {
    bson_return_if_fail(observer);
    bson_free(observer);
}

/*
 *--------------------------------------------------------------------------
 *
 * The following are trigger functions for various callbacks defined in the
 * clientObserverTable. These trigger functions are meant to be called
 * internally only.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_client_observer_trigger_command (mongoc_client_observer_t *table,
                                        const bson_t *command,
                                        char ns[])
{
    mongoc_client_observer_command_t callback =
        (mongoc_client_observer_command_t)
        table->callbacks[MONGOC_CLIENT_OBSERVER_COMMAND];

    if (callback) {
        callback(command, ns, table->user_data);
    }
}

void
mongoc_client_observer_trigger_socket_bind (mongoc_client_observer_t *table,
                                            mongoc_socket_t *sock,
                                            const struct sockaddr *addr)
{
    mongoc_client_observer_socket_bind_t callback =
        (mongoc_client_observer_socket_bind_t)
        table->callbacks[MONGOC_CLIENT_OBSERVER_SOCKET_BIND];

    if (callback) {
        callback(sock, addr, table->user_data);
    }
}

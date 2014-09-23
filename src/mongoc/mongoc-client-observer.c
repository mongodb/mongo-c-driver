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

#include <stdio.h>
#include "mongoc-client-observer.h"

/*
 *--------------------------------------------------------------------------
 *
 * clientObserverTable, responsible for triggering callback functions that
 * are set by a user on meaninful events in the driver. Defaults to a set of
 * no-op callback functions.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_client_observer_t
clientObserverTable =
{
    observer_default_command_callback,
    observer_default_socket_bind_callback,
};

/*
 *--------------------------------------------------------------------------
 *
 * set_custom_observer_t --
 *
 *      Set the client observer table to point to custom callback functions.
 *
 *      All functions outlined in the mongoc_client_observer_t must be
 *      present in @custom_table for this call to succeed. To 'skip' a
 *      callback for some event 'e', pass observer_default_e_callback(),
 *      which will be a no-op, as a placeholder.
 *
 *      If no custom table is set, then all callbacks will be no-ops.
 *
 * Returns:
 *
 *      0 on success, 1 on failure.
 *
 * Side effects:
 *
 *      Custom callback functions will be called when trigger points are
 *      hit in the client.
 *
 *      If 'custom_table' is not a valid mongoc_client_observer_t, this
 *      function will not change the clientObserverTable.
 *
 *--------------------------------------------------------------------------
 */

int
set_custom_observer_t (const mongoc_client_observer_t *custom_table)
{
    clientObserverTable = *custom_table;
    return 0;
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
trigger_command_callback (const bson_t *command, char ns[])
{
    clientObserverTable.on_command(command, ns);
}

void
trigger_socket_action_callback (mongoc_socket_t *sock,
                                const struct sockaddr *addr)
{
    clientObserverTable.on_socket_bind(sock, addr);
}

/*
 *--------------------------------------------------------------------------
 *
 * The following are no-op placeholder functions for our default client
 * observer table. These are meant to be called by the observer only, but
 * may be passed into set_custom_observer_t by the user as placeholders.
 *
 *--------------------------------------------------------------------------
 */

void
observer_default_command_callback (const bson_t *command, char ns[])
{
    return;
}

void
observer_default_socket_bind_callback (mongoc_socket_t *sock,
                                         const struct sockaddr *addr)
{
    return;
}

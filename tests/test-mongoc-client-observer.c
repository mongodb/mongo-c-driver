#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include <stdio.h>

#include "mongoc-cluster-private.h"
#include "mongoc-socket.h"

#include "TestSuite.h"

/* Global counters */
bool cmd_flag_a = false;
bool cmd_flag_b = false;
bool sock_flag  = false;

/* Some test callback functions */

void
command_callback_a(const bson_t *command, char ns[])
{
    cmd_flag_a = true;
}

void
command_callback_b(const bson_t *command, char ns[])
{
    cmd_flag_b = true;
}

void
socket_bind_callback(mongoc_socket_t *sock,
                     const struct sockaddr *addr)
{
    sock_flag = true;
}

/*
 * Trigger all actions that should call our callbacks.
 * These currently are:
 * - running a command
 * - binding a sock_flag to a new address
 */
void
trigger_actions(mongoc_client_t *client,
                mongoc_socket_t *sock,
                struct sockaddr_in saddr)
{
    const char *db_name = "admin";
    bson_t *command;

    /* reset our test flags */
    cmd_flag_a = false;
    cmd_flag_b = false;
    sock_flag = false;

    /* run a command */
    command = bson_new();
    bson_append_int32(command, "ismaster", -1, 1);
    mongoc_client_command_simple(client, db_name, command,
                                 NULL, NULL, NULL);

    /* bind a socket */
    mongoc_socket_bind(sock, (struct sockaddr *)&saddr, sizeof saddr);
    mongoc_socket_close(sock);

    bson_destroy(command);
    return;
}

static void
test_mongoc_client_observer_basic (void)
{
    mongoc_client_t *client;
    mongoc_socket_t *sock;
    mongoc_client_observer_t *observer;
    struct sockaddr_in saddr;

    /* Sets of callback funtions for testing */
    mongoc_client_observer_function_t table[] =
        {
            { MONGOC_CLIENT_OBSERVER_COMMAND, (mongoc_client_observer_callback_t)command_callback_a },
            { MONGOC_CLIENT_OBSERVER_SOCKET_BIND, NULL }
        };

    client = mongoc_client_new ("mongodb://localhost:27017/");

    sock = mongoc_socket_new(AF_INET, SOCK_STREAM, 0);
    if (!sock) {
        printf("Couldn't create a socket\n");
        return;
    }
    memset (&saddr, 0, sizeof saddr);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(12345);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* for sanity, trigger actions, check counters */
    trigger_actions(client, sock, saddr);
    assert(!cmd_flag_a && !cmd_flag_b && !sock_flag);

    /* hook up two custom functions */
    observer = mongoc_client_observer_new(table, 2, NULL);
    mongoc_client_set_observer(client, observer);

    /* trigger actions, check counters */
    trigger_actions(client, sock, saddr);
    assert(cmd_flag_a);
    assert(!cmd_flag_b && !sock_flag);

    /* clean up */
    mongoc_client_destroy(client);
    mongoc_client_observer_destroy(observer);
    mongoc_cleanup();
}


void
test_client_observer_install (TestSuite *suite)
{
    TestSuite_Add (suite, "/ClientObserver/Basic",
                   test_mongoc_client_observer_basic);
}

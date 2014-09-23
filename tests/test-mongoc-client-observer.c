#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include <stdio.h>

#include "mongoc-cluster-private.h"
#include "mongoc-socket.h"

#include "TestSuite.h"

// Global counters
bool cmd_flag_a = false;
bool cmd_flag_b = false;
bool sock_flag  = false;

// Some test callback functions

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

// Trigger all actions that should call our callbacks.
// These currently are:
// - running a command
// - binding a sock_flag to a new address
void
trigger_actions(mongoc_client_t *client,
                mongoc_socket_t *sock,
                struct sockaddr_in saddr)
{
    const char *db_name = "admin";
    bson_t *command;

    // reset our test flags
    cmd_flag_a = false;
    cmd_flag_b = false;
    sock_flag = false;

    // run a command
    command = bson_new();
    bson_append_int32(command, "ismaster", -1, 1);
    mongoc_client_command_simple(client, db_name, command,
                                 NULL, NULL, NULL);

    // bind a socket
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
    struct sockaddr_in saddr;
    const mongoc_client_observer_t table_a = {
        command_callback_a,
        observer_default_socket_bind_callback,
    };
    const mongoc_client_observer_t table_b = {
        command_callback_b,
        socket_bind_callback,
    };
    const mongoc_client_observer_t default_table = {
        observer_default_command_callback,
        observer_default_socket_bind_callback,
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

    // for sanity, trigger actions, check counters
    trigger_actions(client, sock, saddr);
    assert(!cmd_flag_a && !cmd_flag_b && !sock_flag);

    // hook up one custom function, one default
    set_custom_observer_t(&table_a);

    // trigger actions, check counters
    trigger_actions(client, sock, saddr);
    assert(cmd_flag_a);
    assert(!cmd_flag_b && !sock_flag);

    // now hook up two custom functions
    set_custom_observer_t(&table_b);

    // trigger actions, check counters
    trigger_actions(client, sock, saddr);
    assert(!cmd_flag_a);
    assert(cmd_flag_b && sock_flag);

    // restore table to default
    set_custom_observer_t(&default_table);

    // trigger actions, check counters
    trigger_actions(client, sock, saddr);
    assert(!cmd_flag_a && !cmd_flag_b && !sock_flag);

    mongoc_client_destroy(client);
    mongoc_cleanup();
}


void
test_client_observer_install (TestSuite *suite)
{
    TestSuite_Add (suite, "/ClientObserver/Basic",
                   test_mongoc_client_observer_basic);
}

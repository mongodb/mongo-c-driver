/* mongo.h */

/*    Copyright 2009, 2010 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _MONGO_H_
#define _MONGO_H_

#include "mongo_except.h"
#include "bson.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#define mongo_close_socket(sock) ( closesocket(sock) )
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#define mongo_close_socket(sock) ( close(sock) )
#endif

#if defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || _POSIX_C_SOURCE >= 1
#define _MONGO_USE_GETADDRINFO
#endif

MONGO_EXTERN_C_START

typedef struct mongo_host_port {
    char host[255];
    int port;
    struct mongo_host_port* next;
} mongo_host_port;

typedef struct {
    mongo_host_port* seeds; /*< The list of seed nodes provided by the user. */
    mongo_host_port* hosts; /*< The list of host and ports reported by the replica set */
    char* name;             /*< The name of the replica set. */
    bson_bool_t primary_connected; /*< Whether we've managed to connect to a primary node. */
} mongo_replset;

typedef struct {
    mongo_host_port* primary;
    mongo_replset* replset;
    int sock;
    bson_bool_t connected;
    mongo_exception_context exception;
} mongo_connection;

#pragma pack(1)
typedef struct {
    int len;
    int id;
    int responseTo;
    int op;
} mongo_header;

typedef struct {
    mongo_header head;
    char data;
} mongo_message;

typedef struct {
    int flag; /* non-zero on failure */
    int64_t cursorID;
    int start;
    int num;
} mongo_reply_fields;

typedef struct {
    mongo_header head;
    mongo_reply_fields fields;
    char objs;
} mongo_reply;
#pragma pack()

typedef struct {
    mongo_reply * mm; /* message is owned by cursor */
    mongo_connection * conn; /* connection is *not* owned by cursor */
    const char* ns; /* owned by cursor */
    bson current;
} mongo_cursor;

enum mongo_operations {
    mongo_op_msg = 1000,    /* generic msg command followed by a string */
    mongo_op_update = 2001, /* update object */
    mongo_op_insert = 2002,
    mongo_op_query = 2004,
    mongo_op_get_more = 2005,
    mongo_op_delete = 2006,
    mongo_op_kill_cursors = 2007
};


/*
 * CONNECTIONS
 */
typedef enum {
    mongo_conn_success = 0,
    mongo_conn_bad_arg,
    mongo_conn_no_socket,
    mongo_conn_fail,
    mongo_conn_not_master, /* leaves conn connected to slave */
    mongo_conn_bad_set_name, /* The provided replica set name doesn't match the existing replica set */
    mongo_conn_cannot_find_primary
} mongo_conn_return;

/*
 * Connect to a single MongoDB server.
 *
 * @param conn a mongo_connection object.
 * @param host a numerical network address or a network hostname.
 * @param port the port to connect to.
 *
 * @return mongo_conn_return
 */
mongo_conn_return mongo_connect( mongo_connection * conn , const char* host, int port );

/* 
 * Initialize a connection object for connecting with a replica set.
 *
 * @param conn a mongo_connection object.
 * @param name the name of the replica set to connect to.
 * */
void mongo_replset_init_conn( mongo_connection* conn, const char* name );

/*
 * Add a seed node to the connection object.
 *
 * You must specify at least one seed node before connecting to a replica set.
 *
 * @param conn a mongo_connection object.
 * @param host a numerical network address or a network hostname.
 * @param port the port to connect to.
 */
int mongo_replset_add_seed( mongo_connection* conn, const char* host, int port );

/*
 * Connect to a replica set.
 *
 * Before passing a connection object to this method, you must already have called
 * mongo_replset_init_conn and mongo_replset_add_seed.
 *
 * @param conn a mongo_connection object.
 *
 * @return mongo_conn_return
 */
mongo_conn_return mongo_replset_connect( mongo_connection* conn );

/*
 * Try reconnecting to the server using the existing connection settings.
 *
 * This method will disconnect the current socket. If you've authentication,
 * you'll need to re-authenticate after calling this function.
 *
 * @param conn
 *
 * @return mongo_conn_return
 */
mongo_conn_return mongo_reconnect( mongo_connection * conn );

/*
 * Close the current connection to the server.
 */
bson_bool_t mongo_disconnect( mongo_connection * conn );

/*
 * Close any existing connection to the server and free all allocated
 * memory associated with the conn object.
 *
 * You must always call this method when finished with the connection object.
 *
 * @param conn
 *
 * @return bson_bool_t
 */
bson_bool_t mongo_destroy( mongo_connection * conn );

/* ----------------------------
   CORE METHODS - insert update remove query getmore
   ------------------------------ */

void mongo_insert( mongo_connection * conn , const char * ns , bson * data );
void mongo_insert_batch( mongo_connection * conn , const char * ns , bson ** data , int num );

static const int MONGO_UPDATE_UPSERT = 0x1;
static const int MONGO_UPDATE_MULTI = 0x2;
void mongo_update(mongo_connection* conn, const char* ns, const bson* cond, const bson* op, int flags);

void mongo_remove(mongo_connection* conn, const char* ns, const bson* cond);

mongo_cursor* mongo_find(mongo_connection* conn, const char* ns, bson* query, bson* fields ,int nToReturn ,int nToSkip, int options);
bson_bool_t mongo_cursor_next(mongo_cursor* cursor);
void mongo_cursor_destroy(mongo_cursor* cursor);

/* out can be NULL if you don't care about results. useful for commands */
bson_bool_t mongo_find_one(mongo_connection* conn, const char* ns, bson* query, bson* fields, bson* out);

int64_t mongo_count(mongo_connection* conn, const char* db, const char* coll, bson* query);

/* ----------------------------
   HIGHER LEVEL - indexes - command helpers eval
   ------------------------------ */

/* Returns true on success */
/* WARNING: Unlike other drivers these do not cache results */

static const int MONGO_INDEX_UNIQUE = 0x1;
static const int MONGO_INDEX_DROP_DUPS = 0x2;
bson_bool_t mongo_create_index(mongo_connection * conn, const char * ns, bson * key, int options, bson * out);
bson_bool_t mongo_create_simple_index(mongo_connection * conn, const char * ns, const char* field, int options, bson * out);

/* ----------------------------
   COMMANDS
   ------------------------------ */

bson_bool_t mongo_run_command(mongo_connection * conn, const char * db, bson * command, bson * out);

/* for simple commands with a single k-v pair */
bson_bool_t mongo_simple_int_command(mongo_connection * conn, const char * db, const char* cmd,         int arg, bson * out);
bson_bool_t mongo_simple_str_command(mongo_connection * conn, const char * db, const char* cmd, const char* arg, bson * out);

bson_bool_t mongo_cmd_drop_db(mongo_connection * conn, const char * db);
bson_bool_t mongo_cmd_drop_collection(mongo_connection * conn, const char * db, const char * collection, bson * out);

void mongo_cmd_add_user(mongo_connection* conn, const char* db, const char* user, const char* pass);
bson_bool_t mongo_cmd_authenticate(mongo_connection* conn, const char* db, const char* user, const char* pass);

/* return value is master status */
bson_bool_t mongo_cmd_ismaster(mongo_connection * conn, bson * out);

/* true return indicates error */
bson_bool_t mongo_cmd_get_last_error(mongo_connection * conn, const char * db, bson * out);
bson_bool_t mongo_cmd_get_prev_error(mongo_connection * conn, const char * db, bson * out);
void        mongo_cmd_reset_error(mongo_connection * conn, const char * db);

/* ----------------------------
   UTILS
   ------------------------------ */

MONGO_EXTERN_C_END


#endif

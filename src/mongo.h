/* mongo.h */

#ifndef _MONGO_H_
#define _MONGO_H_

#include "mongo_except.h"
#include "bson.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

typedef struct mongo_connection_options {
    char host[255];
    int port;
} mongo_connection_options;

typedef struct {
    mongo_connection_options options;
    struct sockaddr_in sa;
    socklen_t addressSize;
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


/* ----------------------------
   CONNECTION STUFF
   ------------------------------ */

/**
 * @param options can be null
 * return of 0 indicates success
 */
int mongo_connect( mongo_connection * conn , mongo_connection_options * options );
bson_bool_t mongo_disconnect( mongo_connection * conn );
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

/* true return indicates error */
bson_bool_t mongo_cmd_get_last_error(mongo_connection * conn, const char * db, bson * out);
bson_bool_t mongo_cmd_get_prev_error(mongo_connection * conn, const char * db, bson * out);
void        mongo_cmd_reset_error(mongo_connection * conn, const char * db);

/* ----------------------------
   UTILS
   ------------------------------ */



#endif

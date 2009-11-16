/* mongo.h */

#ifndef _MONGO_H_
#define _MONGO_H_

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
bson_bool_t mongo_destory( mongo_connection * conn );



/* ----------------------------
   CORE METHODS - insert update remove query getmore
   ------------------------------ */

void mongo_insert( mongo_connection * conn , const char * ns , bson * data );
void mongo_insert_batch( mongo_connection * conn , const char * ns , bson ** data , int num );

static const int MONGO_UPDATE_UPSERT = 0x1;
static const int MONGO_UPDATE_MULTI = 0x2;
void mongo_update(mongo_connection* conn, const char* ns, const bson* cond, const bson* op, int flags);

mongo_cursor* mongo_find(mongo_connection* conn, const char* ns, bson* query, bson* fields ,int nToReturn ,int nToSkip, int options);
bson_bool_t mongo_cursor_next(mongo_cursor* cursor);
void mongo_cursor_destroy(mongo_cursor* cursor);

bson_bool_t mongo_find_one(mongo_connection* conn, const char* ns, bson* query, bson* fields, bson* out);

/* ----------------------------
   HIGHER LEVEL - indexes - command helpers eval
   ------------------------------ */

/* ----------------------------
   COMMANDS
   ------------------------------ */

/* out must be empty or NULL */
bson_bool_t mongo_run_command(mongo_connection * conn, const char * db, bson * command, bson * out);
bson_bool_t mongo_cmd_drop_db(mongo_connection * conn, const char * db);
bson_bool_t mongo_cmd_drop_collection(mongo_connection * conn, const char * db, const char * collection, bson * out);

/* ----------------------------
   UTILS
   ------------------------------ */



#endif

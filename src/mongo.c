/* mongo.c */

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

#include "mongo.h"
#include "md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#endif

/* only need one of these */
static const int zero = 0;
static const int one = 1;

/* ----------------------------
   message stuff
   ------------------------------ */

static int looping_write(mongo_connection * conn, const void* buf, int len){
    const char* cbuf = buf;
    while (len){
        int sent = send(conn->sock, cbuf, len, 0);
        if (sent == -1)
           return MONGO_IO_ERROR;
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

static int looping_read(mongo_connection * conn, void* buf, int len){
    char* cbuf = buf;
    while (len){
        int sent = recv(conn->sock, cbuf, len, 0);
        if (sent == 0 || sent == -1)
            return MONGO_IO_ERROR;
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

/* Always calls free(mm) */
int mongo_message_send(mongo_connection * conn, mongo_message* mm){
    mongo_header head; /* little endian */
    int res;
    bson_little_endian32(&head.len, &mm->head.len);
    bson_little_endian32(&head.id, &mm->head.id);
    bson_little_endian32(&head.responseTo, &mm->head.responseTo);
    bson_little_endian32(&head.op, &mm->head.op);

    res = looping_write(conn, &head, sizeof(head));
    if( res != MONGO_OK ) {
        free( mm );
        return res;
    }

    res = looping_write(conn, &mm->data, mm->head.len - sizeof(head));
    if( res != MONGO_OK ) {
        free( mm );
        return res;
    }

    free( mm );
    return MONGO_OK;
}

char * mongo_data_append( char * start , const void * data , int len ){
    memcpy( start , data , len );
    return start + len;
}

char * mongo_data_append32( char * start , const void * data){
    bson_little_endian32( start , data );
    return start + 4;
}

char * mongo_data_append64( char * start , const void * data){
    bson_little_endian64( start , data );
    return start + 8;
}

mongo_message * mongo_message_create( int len , int id , int responseTo , int op ){
    mongo_message * mm = (mongo_message*)bson_malloc( len );

    if (!id)
        id = rand();

    /* native endian (converted on send) */
    mm->head.len = len;
    mm->head.id = id;
    mm->head.responseTo = responseTo;
    mm->head.op = op;

    return mm;
}

/* ----------------------------
   connection stuff
   ------------------------------ */
#ifdef _MONGO_USE_GETADDRINFO
static int mongo_socket_connect( mongo_connection * conn, const char * host, int port ){

    struct addrinfo* addrs = NULL;
    struct addrinfo hints;
    char port_str[12];
    int ret;

    conn->sock = 0;
    conn->connected = 0;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET;

    sprintf( port_str, "%d", port );

    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock < 0 ){
        printf("Socket: %d", conn->sock);
        mongo_close_socket( conn->sock );
        return mongo_conn_no_socket;
    }

    ret = getaddrinfo( host, port_str, &hints, &addrs );
    if(ret) {
        fprintf( stderr, "getaddrinfo failed: %s", gai_strerror( ret ) );
        return mongo_conn_fail;
    }

    if ( connect( conn->sock, addrs->ai_addr, addrs->ai_addrlen ) ){
        mongo_close_socket( conn->sock );
        freeaddrinfo( addrs );
        return mongo_conn_fail;
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one) );

    conn->connected = 1;
    freeaddrinfo( addrs );
    return MONGO_OK;
}
#else
static int mongo_socket_connect( mongo_connection * conn, const char * host, int port ){
    struct sockaddr_in sa;
    socklen_t addressSize;

    memset( sa.sin_zero , 0 , sizeof( sa.sin_zero ) );
    sa.sin_family = AF_INET;
    sa.sin_port = htons( port );
    sa.sin_addr.s_addr = inet_addr( host );
    addressSize = sizeof( sa );

    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock < 0 ){
        mongo_close_socket( conn->sock );
        return mongo_conn_no_socket;
    }

    if ( connect( conn->sock, (struct sockaddr *)&sa, addressSize ) ){
        return mongo_conn_fail;
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one) );

    conn->connected = 1;
    return MONGO_OK;
}
#endif

mongo_conn_return mongo_connect( mongo_connection * conn , const char * host, int port ){
    conn->replset = NULL;

    conn->primary = bson_malloc( sizeof( mongo_host_port ) );

    strncpy( conn->primary->host, host, strlen( host ) + 1 );
    conn->primary->port = port;
    conn->primary->next = NULL;

    conn->err = 0;
    conn->errstr = NULL;
    conn->lasterrcode = 0;
    conn->lasterrstr = NULL;

    return mongo_socket_connect(conn, host, port);
}

void mongo_replset_init_conn( mongo_connection* conn, const char* name ) {
    conn->replset = bson_malloc( sizeof( mongo_replset ) );
    conn->replset->primary_connected = 0;
    conn->replset->seeds = NULL;
    conn->replset->hosts = NULL;
    conn->replset->name = (char *)bson_malloc( sizeof( name ) + 1 );
    memcpy( conn->replset->name, name, sizeof( name ) + 1  );

    conn->primary = bson_malloc( sizeof( mongo_host_port ) );
    conn->primary = NULL;

    conn->err = 0;
    conn->errstr = NULL;
    conn->lasterrcode = 0;
    conn->lasterrstr = NULL;
}

static int mongo_replset_add_node( mongo_host_port** list, const char* host, int port ) {
    mongo_host_port* host_port = bson_malloc( sizeof( mongo_host_port ) );
    host_port->port = port;
    host_port->next = NULL;
    strncpy( host_port->host, host, strlen(host) + 1 );

    if( *list == NULL )
        *list = host_port;
    else {
        mongo_host_port* p = *list;
        while( p->next != NULL )
          p = p->next;
        p->next = host_port;
    }

    return MONGO_OK;
}

static int mongo_replset_free_list( mongo_host_port** list ) {
    mongo_host_port* node = *list;
    mongo_host_port* prev;

    while( node != NULL ) {
        prev = node;
        node = node->next;
        free(prev);
    }

    *list = NULL;
    return MONGO_OK;
}

int mongo_replset_add_seed(mongo_connection* conn, const char* host, int port) {
    return mongo_replset_add_node( &conn->replset->seeds, host, port );
}

static int mongo_replset_check_seed( mongo_connection* conn ) {
    bson out;
    bson hosts;
    const char* data;
    bson_iterator it;
    bson_iterator it_sub;
    const char* host_string;
    char* host;
    int len, idx, port, split;

    out.data = NULL;
    out.owned = 1;

    hosts.data = NULL;
    hosts.owned = 1;

    if( mongo_simple_int_command(conn, "admin", "ismaster", 1, &out) == MONGO_OK ) {

        if( bson_find( &it, &out, "hosts" ) ) {
            data = bson_iterator_value( &it );
            bson_iterator_init( &it_sub, data );

            /* Iterate over host list, adding each host to the
             * connection's host list.
             */
            while( bson_iterator_next( &it_sub ) ) {
                host_string = bson_iterator_string( &it_sub );
                len = split = idx = 0;

                /* Split the host_port string at the ':' */
                while(1) {
                    if( *(host_string + len) == 0)
                      break;
                    if( *(host_string + len) == ':' )
                      split = len;
                    len++;
                }

                /* If 'split' is set, we know the that port exists;
                 * Otherwise, we set the default port.
                 */
                if( len > 0 ) {
                    idx = split ? split : len;
                    host = (char *)bson_malloc( idx + 1 );
                    memcpy( host, host_string, idx );
                    memcpy( host + idx, "\0", 1 );
                    if( split )
                        port = atoi( host_string + idx + 1 );
                    else
                        port = 27017;

                    mongo_replset_add_node( &conn->replset->hosts, host, port );
                }
            }
        }
    }

    bson_destroy( &out );
    bson_destroy( &hosts );
    mongo_close_socket( conn->sock );
    conn->sock = 0;
    conn->connected = 0;

    return 0;
}

/* Find out whether the current connected node is master, and
 * verify that the node's replica set name matched the provided name
 */
static int mongo_replset_check_host( mongo_connection* conn ) {

    bson out;
    bson_iterator it;
    bson_bool_t ismaster = 0;
    const char* set_name;

    out.data = NULL;
    out.owned = 1;

    if (mongo_simple_int_command(conn, "admin", "ismaster", 1, &out) == MONGO_OK) {
        if( bson_find(&it, &out, "ismaster") )
            ismaster = bson_iterator_bool( &it );

        if( bson_find( &it, &out, "setName" ) ) {
            set_name = bson_iterator_string( &it );
            if( strcmp( set_name, conn->replset->name ) != 0 ) {
                return mongo_conn_bad_set_name;
            }
        }
    }

    bson_destroy( &out );

    if(ismaster) {
        conn->replset->primary_connected = 1;
    }
    else {
        mongo_close_socket( conn->sock );
    }

    return 0;
}

mongo_conn_return mongo_replset_connect(mongo_connection* conn) {

    int connect_error = 0;
    mongo_host_port* node;

    conn->sock = 0;
    conn->connected = 0;

    /* First iterate over the seed nodes to get the canonical list of hosts
     * from the replica set. Break out once we have a host list.
     */
    node = conn->replset->seeds;
    while( node != NULL ) {
        connect_error = mongo_socket_connect( conn, (const char*)&node->host, node->port );

        if( connect_error == 0 ) {
            if ( (connect_error = mongo_replset_check_seed( conn )) )
                return connect_error;
        }

        if( conn->replset->hosts )
            break;

        node = node->next;
    }

    /* Iterate over the host list, checking for the primary node. */
    if( !conn->replset->hosts ) {
        return mongo_conn_cannot_find_primary;
    }
    else {
        node = conn->replset->hosts;

        while( node != NULL ) {
            connect_error = mongo_socket_connect( conn, (const char*)&node->host, node->port );

            if( connect_error == 0 ) {
                if ( (connect_error = mongo_replset_check_host( conn )) )
                    return connect_error;

                /* Primary found, so return. */
                else if( conn->replset->primary_connected )
                     return 0;

                /* No primary, so close the connection. */
                else {
                    mongo_close_socket( conn->sock );
                    conn->sock = 0;
                    conn->connected = 0;
                }
            }

            node = node->next;
        }
    }

    return mongo_conn_cannot_find_primary;
}

mongo_conn_return mongo_reconnect( mongo_connection * conn ){
    int res;
    mongo_disconnect(conn);

    if( conn->replset ) {
        conn->replset->primary_connected = 0;
        mongo_replset_free_list( &conn->replset->hosts );
        conn->replset->hosts = NULL;
        res = mongo_replset_connect( conn );
        return res;
    }
    else
        return mongo_socket_connect( conn, conn->primary->host, conn->primary->port );
}

bson_bool_t mongo_disconnect( mongo_connection * conn ){
    if( ! conn->connected )
        return 1;

    if( conn->replset ) {
        conn->replset->primary_connected = 0;
        mongo_replset_free_list( &conn->replset->hosts );
        conn->replset->hosts = NULL;
        return mongo_replset_connect( conn );
    }

    mongo_close_socket( conn->sock );

    conn->sock = 0;
    conn->connected = 0;

    return 0;
}

bson_bool_t mongo_destroy( mongo_connection * conn ){
    if( conn->replset ) {
        mongo_replset_free_list( &conn->replset->seeds );
        mongo_replset_free_list( &conn->replset->hosts );
        free( conn->replset->name );
        free( conn->replset );
        conn->replset = NULL;
    }

    free( conn->primary );
    free( conn->errstr );
    free( conn->lasterrstr );

    conn->err = 0;
    conn->errstr = NULL;
    conn->lasterrcode = 0;
    conn->lasterrstr = NULL;

    return mongo_disconnect( conn );
}

/*
 * Determine whether this BSON object is valid for the
 * given operation.
 */
static int mongo_bson_valid( mongo_connection * conn, bson* bson, int write ) {
    if( bson->err & BSON_NOT_UTF8 ) {
        conn->err = MONGO_INVALID_BSON;
        return MONGO_ERROR;
    }

    if( write ) {
        if( (bson->err & BSON_FIELD_HAS_DOT) ||
            (bson->err & BSON_FIELD_INIT_DOLLAR) ) {

            conn->err = MONGO_INVALID_BSON;
            return MONGO_ERROR;

        }
    }

    conn->err = 0;
    conn->errstr = NULL;

    return MONGO_OK;
}

int mongo_insert_batch( mongo_connection * conn, const char * ns,
    bson ** bsons, int count ) {

    int size =  16 + 4 + strlen( ns ) + 1;
    int i;
    mongo_message * mm;
    char* data;

    for(i=0; i<count; i++){
        size += bson_size(bsons[i]);
        if( mongo_bson_valid( conn, bsons[i], 1 ) != MONGO_OK )
            return MONGO_ERROR;
    }

    mm = mongo_message_create( size , 0 , 0 , mongo_op_insert );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);

    for(i=0; i<count; i++){
        data = mongo_data_append(data, bsons[i]->data, bson_size( bsons[i] ) );
    }

    return mongo_message_send(conn, mm);
}

int mongo_insert( mongo_connection * conn , const char * ns , bson * bson ) {

    char* data;
    mongo_message* mm;

    /* Make sure that BSON is valid for insert. */
    if( mongo_bson_valid( conn, bson, 1 ) != MONGO_OK ) {
        return MONGO_ERROR;
    }

    mm = mongo_message_create( 16 /* header */
                              + 4 /* ZERO */
                              + strlen(ns)
                              + 1 + bson_size(bson)
                              , 0, 0, mongo_op_insert);

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append(data, bson->data, bson_size(bson));

    return mongo_message_send(conn, mm);
}

int mongo_update(mongo_connection* conn, const char* ns, const bson* cond,
    const bson* op, int flags) {

    char* data;
    mongo_message* mm;

    /* Make sure that the op BSON is valid UTF-8.
     * TODO: decide whether to check cond as well.
     * */
    if( mongo_bson_valid( conn, op, 0 ) != MONGO_OK ) {
        return MONGO_ERROR;
    }

    mm = mongo_message_create( 16 /* header */
                              + 4  /* ZERO */
                              + strlen(ns) + 1
                              + 4  /* flags */
                              + bson_size(cond)
                              + bson_size(op)
                              , 0 , 0 , mongo_op_update );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append32(data, &flags);
    data = mongo_data_append(data, cond->data, bson_size(cond));
    data = mongo_data_append(data, op->data, bson_size(op));

    return mongo_message_send(conn, mm);
}

void mongo_remove(mongo_connection* conn, const char* ns, const bson* cond){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4  /* ZERO */
                                             + strlen(ns) + 1
                                             + 4  /* ZERO */
                                             + bson_size(cond)
                                             , 0 , 0 , mongo_op_delete );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, cond->data, bson_size(cond));

    mongo_message_send(conn, mm);
}

int mongo_read_response( mongo_connection * conn, mongo_reply** mm ){
    mongo_header head; /* header from network */
    mongo_reply_fields fields; /* header from network */
    mongo_reply * out; /* native endian */
    unsigned int len;
    int res;

    looping_read(conn, &head, sizeof(head));
    looping_read(conn, &fields, sizeof(fields));

    bson_little_endian32(&len, &head.len);

    if (len < sizeof(head)+sizeof(fields) || len > 64*1024*1024)
        return MONGO_READ_SIZE_ERROR;  /* most likely corruption */

    out = (mongo_reply*)bson_malloc(len);

    out->head.len = len;
    bson_little_endian32(&out->head.id, &head.id);
    bson_little_endian32(&out->head.responseTo, &head.responseTo);
    bson_little_endian32(&out->head.op, &head.op);

    bson_little_endian32(&out->fields.flag, &fields.flag);
    bson_little_endian64(&out->fields.cursorID, &fields.cursorID);
    bson_little_endian32(&out->fields.start, &fields.start);
    bson_little_endian32(&out->fields.num, &fields.num);

    res = looping_read(conn, &out->objs, len-sizeof(head)-sizeof(fields));
    if( res != MONGO_OK ) {
        free(out);
        return res;
    }

    *mm = out;

    return MONGO_OK;
}

mongo_cursor* mongo_find(mongo_connection* conn, const char* ns, bson* query,
    bson* fields, int nToReturn, int nToSkip, int options) {

    int sl;
    int res;
    volatile mongo_cursor * cursor; /* volatile due to longjmp in mongo exception handler */
    char * data;
    mongo_message * mm = mongo_message_create( 16 + /* header */
                                               4 + /*  options */
                                               strlen( ns ) + 1 + /* ns */
                                               4 + 4 + /* skip,return */
                                               bson_size( query ) +
                                               bson_size( fields ) ,
                                               0 , 0 , mongo_op_query );


    data = &mm->data;
    data = mongo_data_append32( data , &options );
    data = mongo_data_append( data , ns , strlen( ns ) + 1 );
    data = mongo_data_append32( data , &nToSkip );
    data = mongo_data_append32( data , &nToReturn );
    data = mongo_data_append( data , query->data , bson_size( query ) );
    if ( fields )
        data = mongo_data_append( data , fields->data , bson_size( fields ) );

    bson_fatal_msg( (data == ((char*)mm) + mm->head.len), "query building fail!" );

    mongo_message_send( conn , mm );

    cursor = (mongo_cursor*)bson_malloc(sizeof(mongo_cursor));

    res = mongo_read_response( conn, &(cursor->mm) );
    if( res != MONGO_OK ) {
        conn->err = res;
        free((mongo_cursor*)cursor); /* cast away volatile, not changing type */
        return NULL;
    }

    sl = strlen(ns)+1;
    cursor->ns = bson_malloc(sl);
    if (!cursor->ns){
        free(cursor->mm);
        free((mongo_cursor*)cursor); /* cast away volatile, not changing type */
        return NULL;
    }
    memcpy((void*)cursor->ns, ns, sl); /* cast needed to silence GCC warning */
    cursor->conn = conn;
    cursor->current.data = NULL;

    return (mongo_cursor*)cursor;
}

int mongo_find_one(mongo_connection* conn, const char* ns, bson* query,
    bson* fields, bson* out) {

    mongo_cursor* cursor = mongo_find(conn, ns, query, fields, 1, 0, 0);

    if (cursor && mongo_cursor_next(cursor) == MONGO_OK){
        bson_copy(out, &cursor->current);
        mongo_cursor_destroy(cursor);
        return MONGO_OK;
    } else{
        mongo_cursor_destroy(cursor);
        return MONGO_ERROR;
    }
}

int64_t mongo_count(mongo_connection* conn, const char* db, const char* ns, bson* query){
    bson_buffer bb;
    bson cmd;
    bson out;
    int64_t count = -1;

    bson_buffer_init(&bb);
    bson_append_string(&bb, "count", ns);
    if (query && bson_size(query) > 5) /* not empty */
        bson_append_bson(&bb, "query", query);
    bson_from_buffer(&cmd, &bb);

    if( mongo_run_command(conn, db, &cmd, &out) == MONGO_OK ) {
        bson_iterator it;
        if(bson_find(&it, &out, "n"))
            count = bson_iterator_long(&it);
        bson_destroy(&cmd);
        bson_destroy(&out);
        return count;
    }
    else {
        bson_destroy(&cmd);
        return MONGO_ERROR;
    }
}



int mongo_cursor_get_more(mongo_cursor* cursor){
    int res;

    if( ! cursor->mm->fields.cursorID)
        return MONGO_CURSOR_EXHAUSTED;
    else if( ! cursor->mm )
        return MONGO_CURSOR_INVALID;
    else {
        mongo_connection* conn = cursor->conn;
        char* data;
        int sl = strlen(cursor->ns)+1;
        mongo_message * mm = mongo_message_create(16 /*header*/
                                                 +4 /*ZERO*/
                                                 +sl
                                                 +4 /*numToReturn*/
                                                 +8 /*cursorID*/
                                                 , 0, 0, mongo_op_get_more);
        data = &mm->data;
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append(data, cursor->ns, sl);
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append64(data, &cursor->mm->fields.cursorID);
        mongo_message_send(conn, mm);

        free(cursor->mm);

        res = mongo_read_response( cursor->conn, &(cursor->mm) );
        if( res != MONGO_OK ) {
            cursor->mm = NULL;
            mongo_cursor_destroy(cursor);
            return res;
        }

        return MONGO_OK;
    }
}

int mongo_cursor_next(mongo_cursor* cursor){
    char* bson_addr;

    /* no data */
    if (!cursor->mm || cursor->mm->fields.num == 0) {
        return MONGO_ERROR;
    }

    /* first */
    if (cursor->current.data == NULL){
        bson_init(&cursor->current, &cursor->mm->objs, 0);
        return MONGO_OK;
    }

    bson_addr = cursor->current.data + bson_size(&cursor->current);
    if (bson_addr >= ((char*)cursor->mm + cursor->mm->head.len)){
        if( mongo_cursor_get_more(cursor) != MONGO_OK )
            return MONGO_ERROR;
        bson_init(&cursor->current, &cursor->mm->objs, 0);
    } else {
        bson_init(&cursor->current, bson_addr, 0);
    }

    return MONGO_OK;
}

int mongo_cursor_destroy(mongo_cursor* cursor){
    int result = MONGO_OK;

    if (!cursor) return result;

    if (cursor->mm && cursor->mm->fields.cursorID){
        mongo_connection* conn = cursor->conn;
        mongo_message * mm = mongo_message_create(16 /*header*/
                                                 +4 /*ZERO*/
                                                 +4 /*numCursors*/
                                                 +8 /*cursorID*/
                                                 , 0, 0, mongo_op_kill_cursors);
        char* data = &mm->data;
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append32(data, &one);
        data = mongo_data_append64(data, &cursor->mm->fields.cursorID);

        result = mongo_message_send(conn, mm);

        free(cursor->mm);
        free((void*)cursor->ns);
        free(cursor);
    }

    return result;
}

int mongo_create_index(mongo_connection * conn, const char * ns, bson * key, int options, bson * out){
    bson_buffer bb;
    bson b;
    bson_iterator it;
    char name[255] = {'_'};
    int i = 1;
    char idxns[1024];

    bson_iterator_init(&it, key->data);
    while(i < 255 && bson_iterator_next(&it)){
        strncpy(name + i, bson_iterator_key(&it), 255 - i);
        i += strlen(bson_iterator_key(&it));
    }
    name[254] = '\0';

    bson_buffer_init(&bb);
    bson_append_bson(&bb, "key", key);
    bson_append_string(&bb, "ns", ns);
    bson_append_string(&bb, "name", name);
    if (options & MONGO_INDEX_UNIQUE)
        bson_append_bool(&bb, "unique", 1);
    if (options & MONGO_INDEX_DROP_DUPS)
        bson_append_bool(&bb, "dropDups", 1);

    bson_from_buffer(&b, &bb);

    strncpy(idxns, ns, 1024-16);
    strcpy(strchr(idxns, '.'), ".system.indexes");
    mongo_insert(conn, idxns, &b);
    bson_destroy(&b);

    *strchr(idxns, '.') = '\0'; /* just db not ns */
    return mongo_cmd_get_last_error(conn, idxns, out);
}
bson_bool_t mongo_create_simple_index(mongo_connection * conn, const char * ns, const char* field, int options, bson * out){
    bson_buffer bb;
    bson b;
    bson_bool_t success;

    bson_buffer_init(&bb);
    bson_append_int(&bb, field, 1);
    bson_from_buffer(&b, &bb);

    success = mongo_create_index(conn, ns, &b, options, out);
    bson_destroy(&b);
    return success;
}

int mongo_run_command(mongo_connection* conn, const char* db, bson* command,
    bson* out) {

    bson fields;
    int sl = strlen(db);
    char* ns = bson_malloc(sl + 5 + 1); /* ".$cmd" + nul */
    int res;

    strcpy(ns, db);
    strcpy(ns+sl, ".$cmd");

    res = mongo_find_one(conn, ns, command, bson_empty(&fields), out);
    free(ns);
    return res;
}

int mongo_simple_int_command(mongo_connection * conn, const char * db,
    const char* cmdstr, int arg, bson * realout) {

    bson out;
    bson cmd;
    bson_buffer bb;
    bson_bool_t success = 0;

    bson_buffer_init(&bb);
    bson_append_int(&bb, cmdstr, arg);
    bson_from_buffer(&cmd, &bb);

    if( mongo_run_command(conn, db, &cmd, &out) == MONGO_OK ){
        bson_iterator it;
        if(bson_find(&it, &out, "ok"))
            success = bson_iterator_bool(&it);
    }

    bson_destroy(&cmd);

    if (realout)
        *realout = out;
    else
        bson_destroy(&out);

    if( success )
      return MONGO_OK;
    else {
      conn->err = MONGO_COMMAND_FAILED;
      return MONGO_ERROR;
    }
}

int mongo_simple_str_command(mongo_connection * conn, const char * db,
    const char* cmdstr, const char* arg, bson * realout) {

    bson out;
    bson cmd;
    bson_buffer bb;
    int success = 0;

    bson_buffer_init(&bb);
    bson_append_string(&bb, cmdstr, arg);
    bson_from_buffer(&cmd, &bb);

    if( mongo_run_command(conn, db, &cmd, &out) == MONGO_OK ) {
        bson_iterator it;
        if(bson_find(&it, &out, "ok"))
            success = bson_iterator_bool(&it);
    }

    bson_destroy(&cmd);

    if (realout)
        *realout = out;
    else
        bson_destroy(&out);

    if(success)
      return MONGO_OK;
    else
      return MONGO_ERROR;
}

int mongo_cmd_drop_db(mongo_connection * conn, const char * db){
    return mongo_simple_int_command(conn, db, "dropDatabase", 1, NULL);
}

int mongo_cmd_drop_collection(mongo_connection * conn, const char * db, const char * collection, bson * out){
    return mongo_simple_str_command(conn, db, "drop", collection, out);
}

void mongo_cmd_reset_error(mongo_connection * conn, const char * db){
    mongo_simple_int_command(conn, db, "reseterror", 1, NULL);
}

static int mongo_cmd_get_error_helper(mongo_connection * conn, const char * db,
    bson * realout, const char * cmdtype) {

    bson out = {NULL,0};
    bson_bool_t haserror = 0;

    /* Reset last error codes. */
    conn->lasterrcode = 0;
    free(conn->lasterrstr);
    conn->lasterrstr = NULL;

    /* If there's an error, store its code and string in the connection object. */
    if( mongo_simple_int_command(conn, db, cmdtype, 1, &out) == MONGO_OK ) {
        bson_iterator it;
        haserror = (bson_find(&it, &out, "err") != bson_null);
        if( haserror ) {
             conn->lasterrstr = (char *)bson_malloc( bson_iterator_string_len( &it ) );
             if( conn->lasterrstr ) {
                 strcpy( conn->lasterrstr, bson_iterator_string( &it ) );
             }

            if( bson_find( &it, &out, "code" ) != bson_null )
                conn->lasterrcode = bson_iterator_int( &it );
        }
    }

    if(realout)
        *realout = out; /* transfer of ownership */
    else
        bson_destroy(&out);

    if( haserror )
        return MONGO_ERROR;
    else
        return MONGO_OK;
}

int mongo_cmd_get_prev_error(mongo_connection * conn, const char * db, bson * out) {
    return mongo_cmd_get_error_helper(conn, db, out, "getpreverror");
}

int mongo_cmd_get_last_error(mongo_connection * conn, const char * db, bson * out) {
    return mongo_cmd_get_error_helper(conn, db, out, "getlasterror");
}

bson_bool_t mongo_cmd_ismaster(mongo_connection * conn, bson * realout){
    bson out = {NULL,0};
    bson_bool_t ismaster = 0;

    if (mongo_simple_int_command(conn, "admin", "ismaster", 1, &out) == MONGO_OK){
        bson_iterator it;
        bson_find(&it, &out, "ismaster");
        ismaster = bson_iterator_bool(&it);
    }

    if(realout)
        *realout = out; /* transfer of ownership */
    else
        bson_destroy(&out);

    return ismaster;
}

static void digest2hex(mongo_md5_byte_t digest[16], char hex_digest[33]){
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int i;
    for (i=0; i<16; i++){
        hex_digest[2*i]     = hex[(digest[i] & 0xf0) >> 4];
        hex_digest[2*i + 1] = hex[ digest[i] & 0x0f      ];
    }
    hex_digest[32] = '\0';
}

static void mongo_pass_digest(const char* user, const char* pass, char hex_digest[33]){
    mongo_md5_state_t st;
    mongo_md5_byte_t digest[16];

    mongo_md5_init(&st);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)user, strlen(user));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)":mongo:", 7);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)pass, strlen(pass));
    mongo_md5_finish(&st, digest);
    digest2hex(digest, hex_digest);
}

int mongo_cmd_add_user(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson user_obj;
    bson pass_obj;
    char hex_digest[33];
    char* ns = bson_malloc(strlen(db) + strlen(".system.users") + 1);
    int res;

    strcpy(ns, db);
    strcpy(ns+strlen(db), ".system.users");

    mongo_pass_digest(user, pass, hex_digest);

    bson_buffer_init(&bb);
    bson_append_string(&bb, "user", user);
    bson_from_buffer(&user_obj, &bb);

    bson_buffer_init(&bb);
    bson_append_start_object(&bb, "$set");
    bson_append_string(&bb, "pwd", hex_digest);
    bson_append_finish_object(&bb);
    bson_from_buffer(&pass_obj, &bb);


    res = mongo_update(conn, ns, &user_obj, &pass_obj, MONGO_UPDATE_UPSERT);

    free(ns);
    bson_destroy(&user_obj);
    bson_destroy(&pass_obj);

    return res;
}

bson_bool_t mongo_cmd_authenticate(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson from_db, auth_cmd;
    const char* nonce;
    bson_bool_t success = 0;

    mongo_md5_state_t st;
    mongo_md5_byte_t digest[16];
    char hex_digest[33];

    if( mongo_simple_int_command(conn, db, "getnonce", 1, &from_db) == MONGO_OK ) {
        bson_iterator it;
        bson_find(&it, &from_db, "nonce");
        nonce = bson_iterator_string(&it);
    }
    else {
        return MONGO_ERROR;
    }

    mongo_pass_digest(user, pass, hex_digest);

    mongo_md5_init(&st);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)nonce, strlen(nonce));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)user, strlen(user));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)hex_digest, 32);
    mongo_md5_finish(&st, digest);
    digest2hex(digest, hex_digest);

    bson_buffer_init(&bb);
    bson_append_int(&bb, "authenticate", 1);
    bson_append_string(&bb, "user", user);
    bson_append_string(&bb, "nonce", nonce);
    bson_append_string(&bb, "key", hex_digest);
    bson_from_buffer(&auth_cmd, &bb);

    bson_destroy(&from_db);

    if( mongo_run_command(conn, db, &auth_cmd, &from_db) == MONGO_OK ) {
        bson_iterator it;
        if(bson_find(&it, &from_db, "ok"))
            success = bson_iterator_bool(&it);
    }

    bson_destroy(&from_db);
    bson_destroy(&auth_cmd);

    if( success )
        return MONGO_OK;
    else
        return MONGO_ERROR;
}

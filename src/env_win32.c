/* env_win32.c */

/*    Copyright 2009-2012 10gen Inc.
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

/* Networking and other niceties for WIN32. */
#include "env.h"
#include <string.h>

#ifdef _MSC_VER
#include <ws2tcpip.h>  // send,recv,socklen_t etc
#include <wspiapi.h>   // addrinfo
#else
#include <windows.h>
#include <winsock.h>
typedef int socklen_t;
#endif

#ifndef NI_MAXSERV
# define NI_MAXSERV 32
#endif

static void mongo_clear_errors( mongo *conn ) {
    conn->err = 0;
    memset( conn->errstr, 0, MONGO_ERR_LEN );
}

static void mongo_set_error( mongo *conn, int err, const char *str ) {
    int errstr_size, str_size;

    conn->err = err;
    conn->errcode = WSAGetLastError();

    if( str ) {
        str_size = strlen( str ) + 1;
        errstr_size = str_size > MONGO_ERR_LEN ? MONGO_ERR_LEN : str_size;
        memcpy( conn->errstr, str, errstr_size );
        conn->errstr[errstr_size] = '\0';
    }
}

int mongo_close_socket( int socket ) {
    return closesocket( socket );
}

int mongo_write_socket( mongo *conn, const void *buf, int len ) {
    const char *cbuf = buf;
    int flags = 0;

    while ( len ) {
        int sent = send( conn->sock, cbuf, len, flags );
        if ( sent == -1 ) {
	    mongo_set_error( conn, MONGO_IO_ERROR, NULL );
	    conn->connected = 0;
            return MONGO_ERROR;
        }
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

int mongo_read_socket( mongo *conn, void *buf, int len ) {
    char *cbuf = buf;
    while ( len ) {
        int sent = recv( conn->sock, cbuf, len, 0 );
        if ( sent == 0 || sent == -1 ) {
	    mongo_set_error( conn, MONGO_IO_ERROR, NULL );
            return MONGO_ERROR;
        }
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

/* This is a no-op in the generic implementation. */
int mongo_set_socket_op_timeout( mongo *conn, int millis ) {
    return MONGO_OK;
}

int mongo_socket_connect( mongo *conn, const char *host, int port ) {
    char port_str[NI_MAXSERV];
    char errstr[MONGO_ERR_LEN];
    int status;

    struct addrinfo ai_hints;
    struct addrinfo *ai_list = NULL;
    struct addrinfo *ai_ptr = NULL;

    conn->sock = 0;
    conn->connected = 0;

    bson_sprintf( port_str, "%d", port );

    memset( &ai_hints, 0, sizeof( ai_hints ) );
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_STREAM;
    ai_hints.ai_protocol = IPPROTO_TCP;

    status = getaddrinfo( host, port_str, &ai_hints, &ai_list );
    if ( status != 0 ) {
	bson_sprintf( errstr, "getaddrinfo failed with error %d", status );
	mongo_set_error( conn, MONGO_CONN_ADDR_FAIL, errstr );
        return MONGO_ERROR;
    }

    for ( ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next ) {
        conn->sock = socket( ai_ptr->ai_family, ai_ptr->ai_socktype,
		             ai_ptr->ai_protocol );

        if ( conn->sock < 0 ) {
	    mongo_set_error( conn, MONGO_SOCKET_ERROR, "socket() failed" );
            conn->sock = 0;
            continue;
        }

        status = connect( conn->sock, ai_ptr->ai_addr, ai_ptr->ai_addrlen );
        if ( status != 0 ) {
	    mongo_set_error( conn, MONGO_SOCKET_ERROR, "connect() failed" );
            mongo_close_socket( conn->sock );
            conn->sock = 0;
            continue;
        }

        if ( ai_ptr->ai_protocol == IPPROTO_TCP ) {
            int flag = 1;

            setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY,
                        ( void * ) &flag, sizeof( flag ) );

            if ( conn->op_timeout_ms > 0 )
                mongo_set_socket_op_timeout( conn, conn->op_timeout_ms );
        }

        conn->connected = 1;
        break;
    }

    freeaddrinfo( ai_list );

    if ( ! conn->connected ) {
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    } 
    else {
        mongo_clear_errors( conn );
        return MONGO_OK;
    }
}

MONGO_EXPORT int mongo_env_sock_init( void ) {

    WSADATA wsaData;
    WORD wVers;
    static int called_once;
    static int retval;

    if (called_once) return retval;

    called_once = 1;
    wVers = MAKEWORD(1, 1);
    retval = (WSAStartup(wVers, &wsaData) == 0);

    return retval;
}

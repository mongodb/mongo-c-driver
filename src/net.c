/* net.c */

/*    Copyright 2009-2011 10gen Inc.
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

#include "net.h"

#ifdef _MONGO_USE_GETADDRINFO
int mongo_socket_connect( mongo_connection * conn, const char * host, int port ){

    struct addrinfo* addrs = NULL;
    struct addrinfo hints;
    int flag = 1;
    char port_str[12];
    int ret;

    conn->sock = 0;
    conn->connected = 0;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    sprintf( port_str, "%d", port );

    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock < 0 ){
        printf("Socket: %d", conn->sock);
        mongo_close_socket( conn->sock );
        conn->err = MONGO_CONN_NO_SOCKET;
        return MONGO_ERROR;
    }

    ret = getaddrinfo( host, port_str, &hints, &addrs );
    if(ret) {
        fprintf( stderr, "getaddrinfo failed: %s", gai_strerror( ret ) );
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    }

    if ( connect( conn->sock, addrs->ai_addr, addrs->ai_addrlen ) ){
        mongo_close_socket( conn->sock );
        freeaddrinfo( addrs );
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR:
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );

    conn->connected = 1;
    freeaddrinfo( addrs );

    return MONGO_OK;
}
#else
int mongo_socket_connect( mongo_connection * conn, const char * host, int port ){
    struct sockaddr_in sa;
    socklen_t addressSize;
    int flag = 1;

    memset( sa.sin_zero , 0 , sizeof( sa.sin_zero ) );
    sa.sin_family = AF_INET;
    sa.sin_port = htons( port );
    sa.sin_addr.s_addr = inet_addr( host );
    addressSize = sizeof( sa );

    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock < 0 ){
        mongo_close_socket( conn->sock );
        conn->err = MONGO_CONN_NO_SOCKET;
        return MONGO_ERROR;
    }

    if ( connect( conn->sock, (struct sockaddr *)&sa, addressSize ) ){
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(flag) );

    conn->connected = 1;

    return MONGO_OK;
}
#endif

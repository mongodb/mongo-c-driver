#include <bson.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <mongoc-thread-private.h>

#include "ssl-test.h"

#define TIMEOUT 1000

#define NCLIENTS 5

#define LOCALHOST "127.0.0.1"

typedef enum {
   SSL_TEST_CLIENT_CONNECT,
   SSL_TEST_CLIENT_WRITE_LEN,
   SSL_TEST_CLIENT_WRITE_FOO,
   SSL_TEST_CLIENT_READ_F,
   SSL_TEST_CLIENT_READ_OO,
} ssl_test_client_state_t;

typedef struct
{
   unsigned short          server_port;
   mongoc_stream_t        *sock_stream;
   mongoc_stream_t        *ssl_stream;
   mongoc_socket_t        *conn_sock;
   char                    buf[1024];
   mongoc_iovec_t          riov;
   mongoc_iovec_t          wiov;
   struct sockaddr_in      server_addr;
   ssl_test_client_state_t state;
   int                     len;
   mongoc_cond_t           cond;
   mongoc_mutex_t          cond_mutex;
   ssl_test_result_t      *client_result;
   ssl_test_result_t      *server_result;
} ssl_test_client_t;

typedef struct ssl_test_data
{
   mongoc_ssl_opt_t  *client;
   mongoc_ssl_opt_t  *server;
   const char        *host;
   ssl_test_result_t *client_result;
   ssl_test_result_t *server_result;
   ssl_test_client_t  clients[NCLIENTS];
} ssl_test_data_t;

typedef struct
{
   ssl_test_data_t *data;
   int              n;
} ssl_test_server_t;

/** this function is meant to be run from ssl_test as a child thread
 *
 * It:
 *    1. spins up
 *    2. binds and listens to a random port
 *    3. notifies the client of it's port through a condvar
 *    4. accepts a request
 *    5. reads a 32 bit length
 *    6. reads a string of that length
 *    7. echos it back to the client
 *    8. shuts down
 */
static void *
ssl_test_server (void * ptr)
{
   ssl_test_server_t *server = (ssl_test_server_t *)ptr;
   ssl_test_data_t *data = server->data;

   mongoc_stream_t *sock_stream;
   mongoc_stream_t *ssl_stream;
   mongoc_socket_t *listen_sock;
   mongoc_socket_t *conn_sock;
   socklen_t sock_len;
   char buf[1024];
   ssize_t r;
   mongoc_iovec_t iov;
   struct sockaddr_in server_addr = { 0 };
   int len;

   iov.iov_base = buf;
   iov.iov_len = sizeof buf;

   listen_sock = mongoc_socket_new (AF_INET, SOCK_STREAM, 0);
   assert (listen_sock);

   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
   server_addr.sin_port = htons (0);

   r = mongoc_socket_bind (listen_sock,
                           (struct sockaddr *)&server_addr,
                           sizeof server_addr);
   assert (r == 0);

   sock_len = sizeof(server_addr);
   r = mongoc_socket_getsockname (listen_sock, (struct sockaddr *)&server_addr, &sock_len);
   assert(r == 0);

   r = mongoc_socket_listen (listen_sock, 10);
   assert(r == 0);

   mongoc_mutex_lock(&data->clients[server->n].cond_mutex);
   data->clients[server->n].server_port = ntohs(server_addr.sin_port);
   mongoc_cond_signal(&data->clients[server->n].cond);
   mongoc_mutex_unlock(&data->clients[server->n].cond_mutex);

   conn_sock = mongoc_socket_accept (listen_sock, -1);
   assert (conn_sock);

   sock_stream = mongoc_stream_socket_new (conn_sock);
   assert (sock_stream);
   ssl_stream = mongoc_stream_tls_new(sock_stream, data->server, 0);
   if (!ssl_stream) {
      unsigned long err = ERR_get_error();
      assert(err);

      data->clients[server->n].server_result->ssl_err = err;
      data->clients[server->n].server_result->result = SSL_TEST_SSL_INIT;

      mongoc_stream_destroy (sock_stream);
      mongoc_socket_destroy (listen_sock);

      return NULL;
   }
   assert(ssl_stream);

   r = mongoc_stream_tls_do_handshake (ssl_stream, TIMEOUT);
   if (!r) {
      unsigned long err = ERR_get_error();

      data->clients[server->n].server_result->ssl_err = err;
      data->clients[server->n].server_result->result = SSL_TEST_SSL_HANDSHAKE;

      mongoc_socket_destroy (listen_sock);
      mongoc_stream_destroy(ssl_stream);

      return NULL;
   }

   r = mongoc_stream_readv(ssl_stream, &iov, 1, 4, TIMEOUT);
   if (r < 0) {
#ifdef _WIN32
      assert(errno == WSAETIMEDOUT);
#else
      assert(errno == ETIMEDOUT);
#endif

      data->clients[server->n].server_result->err = errno;
      data->clients[server->n].server_result->result = SSL_TEST_TIMEOUT;

      mongoc_stream_destroy(ssl_stream);
      mongoc_socket_destroy (listen_sock);

      return NULL;
   }

   assert(r == 4);
   memcpy(&len, iov.iov_base, r);

   r = mongoc_stream_readv(ssl_stream, &iov, 1, len, TIMEOUT);
   assert(r == len);

   iov.iov_len = r;
   mongoc_stream_writev(ssl_stream, &iov, 1, TIMEOUT);

   mongoc_stream_destroy(ssl_stream);

   mongoc_socket_destroy (listen_sock);

   data->clients[server->n].server_result->result = SSL_TEST_SUCCESS;

   return NULL;
}

/** this function is meant to be run from ssl_test as a child thread
 *
 * It:
 *    1. spins up
 *    2. waits on a condvar until the server is up
 *    3. connects to the servers port
 *    4. writes a 4 bytes length
 *    5. writes a string of length size
 *    6. reads a response back of the given length
 *    7. confirms that its the same as what was written
 *    8. shuts down
 */
static void *
ssl_test_client (void * ptr)
{
   ssl_test_data_t *data = (ssl_test_data_t *)ptr;
   ssl_test_client_t *client;
   mongoc_stream_poll_t sds[NCLIENTS];
   int errno_captured;
   ssize_t r;
   int i;
   int unfinished = NCLIENTS;

   for (i = 0; i < NCLIENTS; i++) {
      client = data->clients + i;

      client->riov.iov_base = client->buf;
      client->riov.iov_len = sizeof client->buf;

      client->conn_sock = mongoc_socket_new (AF_INET, SOCK_STREAM, 0);
      assert (client->conn_sock);

      mongoc_mutex_lock(&data->clients[i].cond_mutex);
      while (! client->server_port) {
         mongoc_cond_wait(&data->clients[i].cond, &data->clients[i].cond_mutex);
      }
      mongoc_mutex_unlock(&data->clients[i].cond_mutex);

      memset(&client->server_addr, 0, sizeof(client->server_addr));

      client->server_addr.sin_family = AF_INET;
      client->server_addr.sin_port = htons(client->server_port);
      client->server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      r = mongoc_socket_connect (client->conn_sock, (struct sockaddr *)&client->server_addr, sizeof(client->server_addr), 0);
      assert (r == 0 || errno == EAGAIN);

      client->sock_stream = mongoc_stream_socket_new (client->conn_sock);
      assert(client->sock_stream);
      client->ssl_stream = mongoc_stream_tls_new(client->sock_stream, data->client, 1);

      if (! client->ssl_stream) {
         unsigned long err = ERR_get_error();
         assert(err);

         data->clients[i].client_result->ssl_err = err;
         data->clients[i].client_result->result = SSL_TEST_SSL_INIT;

         goto CLEANUP;
      }
      assert(client->ssl_stream);

      sds[i].stream = client->ssl_stream;
      sds[i].events = mongoc_stream_tls_should_read(client->ssl_stream) ? POLLIN : POLLOUT;

      client->state = SSL_TEST_CLIENT_CONNECT;
   }

   while (unfinished) {
      r = mongoc_stream_poll(sds, NCLIENTS, TIMEOUT);
      assert(r > 0);

      for (i = 0; i < NCLIENTS; i++) {
         client = data->clients + i;

         if (! sds[i].revents) {
            continue;
         }

         if (client->state == SSL_TEST_CLIENT_CONNECT) {
            errno = 0;
            r = mongoc_stream_tls_do_handshake (client->ssl_stream, 0);
            errno_captured = errno;

            if (! r && mongoc_stream_tls_should_retry(client->ssl_stream)) {
               sds[i].events = mongoc_stream_tls_should_read(client->ssl_stream) ? POLLIN : POLLOUT;
               continue;
            }

            if (! r) {
               unsigned long err = ERR_get_error();

               if (err) {
                  data->clients[i].client_result->ssl_err = err;
               } else {
                  data->clients[i].client_result->err = errno_captured;
               }

               data->clients[0].client_result->result = SSL_TEST_SSL_HANDSHAKE;

               goto CLEANUP;
            }

            r = mongoc_stream_tls_check_cert (client->ssl_stream, data->host);
            if (! r) {
               data->clients[0].client_result->result = SSL_TEST_SSL_VERIFY;

               goto CLEANUP;
            }

            client->len = 4;

            client->wiov.iov_base = (void *)&client->len;
            client->wiov.iov_len = 4;
            sds[i].events = POLLOUT;
            client->state = SSL_TEST_CLIENT_WRITE_LEN;
         } else if (client->state == SSL_TEST_CLIENT_WRITE_LEN) {
            r = mongoc_stream_writev(client->ssl_stream, &client->wiov, 1, 0);

            assert(r >= 0);

            if (r < client->wiov.iov_len) {
               client->wiov.iov_len -= r;
               client->wiov.iov_base += r;
               continue;
            }

            client->wiov.iov_base = "foo";
            client->wiov.iov_len = 4;
            client->state = SSL_TEST_CLIENT_WRITE_FOO;
         } else if (client->state == SSL_TEST_CLIENT_WRITE_FOO) {
            r = mongoc_stream_writev(client->ssl_stream, &client->wiov, 1, 0);

            assert(r >= 0);

            if (r < client->wiov.iov_len) {
               client->wiov.iov_len -= r;
               client->wiov.iov_base += r;
               continue;
            }

            client->riov.iov_len = 1;
            sds[i].events = POLLIN;
            client->state = SSL_TEST_CLIENT_READ_F;
         } else if (client->state == SSL_TEST_CLIENT_READ_F) {
            r = mongoc_stream_readv(client->ssl_stream, &client->riov, 1, 0, 0);

            assert(r >= 0);

            if (r == 1) {
               assert(memcmp(client->buf, "f", 1) == 0);
            }

            client->riov.iov_len = 3;
            client->state = SSL_TEST_CLIENT_READ_OO;
         } else if (client->state == SSL_TEST_CLIENT_READ_OO) {
            r = mongoc_stream_readv(client->ssl_stream, &client->riov, 1, 0, 0);

            assert(r >= 0);

            if (r) {
               client->riov.iov_base += r;
               client->riov.iov_len -= r;
            }

            if (! client->riov.iov_len) {
               assert(memcmp(client->buf, "oo", 3) == 0);
            }

            unfinished--;
            sds[i].events = 0;
         }
      }
   }

   data->clients[0].client_result->result = SSL_TEST_SUCCESS;

CLEANUP:

   for (i = 0; i < NCLIENTS; i++) {
      client = data->clients + i;
      mongoc_stream_destroy(client->ssl_stream);
   }

   return NULL;
}


/** This is the testing function for the ssl-test lib
 *
 * The basic idea is that you spin up a client and server, which will
 * communicate over a mongoc-stream-tls, with varrying mongoc_ssl_opt's.  The
 * client and server speak a simple echo protocol, so all we're really testing
 * here is that any given configuration suceeds or fails as it should
 */
void
ssl_test (mongoc_ssl_opt_t  *client,
          mongoc_ssl_opt_t  *server,
          const char        *host,
          ssl_test_result_t *client_result,
          ssl_test_result_t *server_result)
{
   ssl_test_data_t data = { 0 };
   mongoc_thread_t threads[NCLIENTS + 1];
   ssl_test_server_t servers[NCLIENTS];
   ssl_test_result_t client_results[NCLIENTS];
   ssl_test_result_t server_results[NCLIENTS];
   int i, r;

   data.server = server;
   data.client = client;
   data.host = host;

   for (i = 0; i < NCLIENTS; i++) {
      mongoc_mutex_init(&data.clients[i].cond_mutex);
      mongoc_cond_init(&data.clients[i].cond);

      if (i == 0) {
         data.clients[i].client_result = client_result;
         data.clients[i].server_result = server_result;
      } else {
         data.clients[i].client_result = client_results + i;
         data.clients[i].server_result = server_results + i;
      }

      servers[i].data = &data;
      servers[i].n = i;
      r = mongoc_thread_create(threads + i, &ssl_test_server, servers + i);
      assert(r == 0);
   }

   r = mongoc_thread_create(threads + i, &ssl_test_client, &data);
   assert(r == 0);

   for (i = 0; i < NCLIENTS + 1; i++) {
      r = mongoc_thread_join(threads[i]);
      assert(r == 0);
   }

   for (i = 0; i < NCLIENTS; i++) {
      mongoc_mutex_destroy(&data.clients[i].cond_mutex);
      mongoc_cond_destroy(&data.clients[i].cond);
   }
}

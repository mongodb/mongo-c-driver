#include <mongoc.h>

#include "mongoc-async-private.h"
#include "mock-server.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async-test"

#define TIMEOUT 10000
#define NSERVERS 10

#define TRUST_DIR "tests/trust_dir"
#define CAFILE TRUST_DIR "/verify/mongo_root.pem"
#define PEMFILE_NOPASS TRUST_DIR "/keys/mongodb.com.pem"

#ifdef _WIN32
static void
usleep (int64_t usec)
{
   HANDLE timer;
   LARGE_INTEGER ft;

   ft.QuadPart = -(10 * usec);

   timer = CreateWaitableTimer (NULL, true, NULL);
   SetWaitableTimer (timer, &ft, 0, NULL, NULL, 0);
   WaitForSingleObject (timer, INFINITE);
   CloseHandle (timer);
}
#endif

static void
test_ismaster_helper (mongoc_async_cmd_result_t result,
                      const bson_t             *bson,
                      int64_t                   rtt_msec,
                      void                     *data,
                      bson_error_t             *error)
{
   int *finished = (int*)data;
   if (result != MONGOC_ASYNC_CMD_SUCCESS) {
      fprintf(stderr, "error: %s\n", error->message);
   }
   assert(result == MONGOC_ASYNC_CMD_SUCCESS);

   (*finished)--;
}

static void
test_ismaster_impl (bool with_ssl)
{
   mock_server_t *servers[NSERVERS];
   mongoc_async_t *async;
   mongoc_stream_t *sock_streams[NSERVERS];
   mongoc_socket_t *conn_sock;
   struct sockaddr_in server_addr = { 0 };
   uint16_t port;
   bool r;
   int i;
   bson_t q = BSON_INITIALIZER;
   int finished = NSERVERS;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t sopt = { 0 };
   mongoc_ssl_opt_t copt = { 0 };
#endif

   port = 20000 + (rand () % 1000);

   for (i = 0; i < NSERVERS; i++) {
      servers[i] = mock_server_new ("127.0.0.1", port + i, NULL, NULL);
      mock_server_set_wire_version (servers[i], 0, i);

#ifdef MONGOC_ENABLE_SSL
      if (with_ssl) {
         sopt.pem_file = PEMFILE_NOPASS;
         sopt.ca_file = CAFILE;

         mock_server_set_ssl_opts (servers[i], &sopt);
      }
#endif

      mock_server_run_in_thread (servers[i]);
   }

   async = mongoc_async_new ();

   usleep (5000);

   for (i = 0; i < NSERVERS; i++) {
      conn_sock = mongoc_socket_new (AF_INET, SOCK_STREAM, 0);
      assert (conn_sock);

      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons (port + i);
      server_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
      r = mongoc_socket_connect (conn_sock, (struct sockaddr *)&server_addr,
                                 sizeof (server_addr), 0);
      assert (r == 0 || errno == EAGAIN);

      sock_streams[i] = mongoc_stream_socket_new (conn_sock);

#ifdef MONGOC_ENABLE_SSL
      if (with_ssl) {
         copt.ca_file = CAFILE;
         copt.weak_cert_validation = 1;

         sock_streams[i] = mongoc_stream_tls_new (sock_streams[i], &copt, 1);
      }
#endif

      bson_append_int32 (&q, "isMaster", 8, 1);

      mongoc_async_cmd (async,
                        sock_streams[i],
                        "admin",
                        &q,
                        &test_ismaster_helper,
                        &finished,
                        TIMEOUT);
   }

   while (mongoc_async_run (async, TIMEOUT)) {
   }

   assert(finished == 0);

   mongoc_async_destroy (async);

   bson_destroy (&q);

   for (i = 0; i < NSERVERS; i++) {
      mock_server_quit (servers[i], 0);
      mock_server_destroy (servers[i]);
      mongoc_stream_destroy (sock_streams[i]);
   }
}


static void
test_ismaster (void)
{
   test_ismaster_impl(false);
}


#ifdef MONGOC_ENABLE_SSL
static void
test_ismaster_ssl (void)
{
   test_ismaster_impl(true);
}
#endif


void
test_async_install (TestSuite *suite)
{
   bool local;

   local = !getenv ("MONGOC_DISABLE_MOCK_SERVER");

   if (local) {
      TestSuite_Add (suite, "/Async/ismaster", test_ismaster);
#ifdef MONGOC_ENABLE_SSL
      TestSuite_Add (suite, "/Async/ismaster_ssl", test_ismaster_ssl);
#endif
   }
}

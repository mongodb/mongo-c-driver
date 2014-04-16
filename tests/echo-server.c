#include <mongoc.h>
#include <mongoc-trace.h>
#include <mongoc-thread-private.h>
#include <stdlib.h>


static void *
client_thread (void *data)
{
   mongoc_stream_t *stream = data;
   mongoc_iovec_t iov;
   ssize_t ret;
   char buf [1024];

   ENTRY;

   BSON_ASSERT (stream);

   while (0 < (ret = mongoc_stream_read (stream, buf, sizeof buf, 0, -1))) {
      iov.iov_base = buf;
      iov.iov_len = ret;
      if (ret != mongoc_stream_writev (stream, &iov, 1, -1)) {
         break;
      }
   }

   mongoc_stream_destroy (stream);

   RETURN (NULL);
}


int
main (int   argc,
      char *argv[])
{
   mongoc_socket_t *server;
   mongoc_socket_t *client;
   mongoc_stream_t *client_stream;
   mongoc_thread_t thread;
   struct sockaddr_in saddr;
   int optval = 1;

   mongoc_init ();

   memset (&saddr, 0, sizeof saddr);
   saddr.sin_family = AF_INET;
   saddr.sin_port = htons (27019);
   saddr.sin_addr.s_addr = htonl (INADDR_ANY);

   server = mongoc_socket_new (AF_INET, SOCK_STREAM, 0);

   if (0 != mongoc_socket_setsockopt (server, SOL_SOCKET, SO_REUSEADDR,
                                      &optval, sizeof optval)) {
      perror ("setsockopt(SOL_SOCKET, SO_REUSEADDR)");
      return EXIT_FAILURE;
   }

   if (0 != mongoc_socket_bind (server,
                                (struct sockaddr *)&saddr,
                                sizeof saddr)) {
      perror ("bind");
      return EXIT_FAILURE;
   }

   if (0 != mongoc_socket_listen (server, 10)) {
      perror ("listen");
      return EXIT_FAILURE;
   }

   for (;;) {
      client = mongoc_socket_accept (server, -1);
      if (!client) {
         perror ("accept");
         continue;
      }

      client_stream = mongoc_stream_socket_new (client);

      mongoc_thread_create (&thread, client_thread, client_stream);

      if (false) break; /* fake noreturn */
   }

   mongoc_cleanup();

   return 0;
}

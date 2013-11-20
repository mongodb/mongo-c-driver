#include <stdio.h>

#include "mongoc-buffer-private.h"
#include "mongoc-rpc-private.h"
#include "mock-server.h"


static bson_t ok;


static void
handler_cb (mock_server_t   *server,
            mongoc_stream_t *stream,
            mongoc_rpc_t    *rpc,
            void            *user_data)
{
   struct iovec *iov;
   mongoc_array_t ar;
   mongoc_rpc_t r = {{ 0 }};
   int iovcnt;

   if ((rpc->header.opcode == MONGOC_OPCODE_QUERY) ||
       (rpc->header.opcode == MONGOC_OPCODE_REPLY)) {
      _mongoc_array_init(&ar, sizeof(struct iovec));

      printf("========\n");
      printf("MsgLen: %d\n", rpc->header.msg_len);
      printf("Request: %d\n", rpc->header.request_id);
      printf("Response: %d\n", rpc->header.response_to);
      printf("OpCode: %d\n", rpc->header.opcode);
      printf("\n");

      r.reply.msg_len = 0;
      r.reply.request_id = -1;
      r.reply.response_to = rpc->header.request_id;
      r.reply.opcode = MONGOC_OPCODE_REPLY;
      r.reply.flags = 0;
      r.reply.cursor_id = 0;
      r.reply.start_from = 0;
      r.reply.n_returned = 1;
      r.reply.documents = bson_get_data(&ok);
      r.reply.documents_len = ok.len;

      _mongoc_rpc_gather(&r, &ar);
      _mongoc_rpc_swab_to_le(&r);

      iov = ar.data;
      iovcnt = ar.len;

      mongoc_stream_writev(stream, iov, iovcnt, -1);

      _mongoc_array_destroy(&ar);
   }
}


int
main (int   argc,
      char *argv[])
{
   mock_server_t *server;
   int ret;

   bson_init(&ok);
   bson_append_double(&ok, "ok", 2, 1.0);
   bson_append_bool(&ok, "ismaster", 8, TRUE);

   server = mock_server_new(NULL, 0, handler_cb, NULL);
   ret = mock_server_run(server);
   mock_server_destroy(server);

   return ret;
}

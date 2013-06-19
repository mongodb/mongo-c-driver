#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-array-private.h>
#include <mongoc-event-private.h>
#include <mongoc-rpc-private.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mongoc-tests.h"


static bson_uint8_t *
get_test_file (const char *filename,
               size_t     *length)
{
   bson_uint32_t len;
   bson_uint8_t *buf;
   char real_filename[256];
   int fd;

   snprintf(real_filename, sizeof real_filename,
            "tests/binary/%s", filename);
   real_filename[sizeof real_filename - 1] = '\0';

   if (-1 == (fd = open(real_filename, O_RDONLY))) {
      fprintf(stderr, "Failed to open: %s\n", real_filename);
      abort();
   }

   len = 4096;
   buf = bson_malloc0(len);
   len = read(fd, buf, len);
   assert(len > 0);

   *length = len;
   return buf;
}


static void
assert_rpc_equal (const char   *filename,
                  mongoc_rpc_t *rpc)
{
   mongoc_array_t ar;
   bson_uint8_t *data;
   struct iovec *iov;
   size_t length;
   off_t off = 0;
   int r;
   int i;

   data = get_test_file(filename, &length);
   mongoc_array_init(&ar, sizeof(struct iovec));
   mongoc_rpc_gather(rpc, &ar);
   mongoc_rpc_swab(rpc);

   for (i = 0; i < ar.len; i++) {
      iov = &mongoc_array_index(&ar, struct iovec, i);
      assert(iov->iov_len <= (length - off));
      r = memcmp(&data[off], iov->iov_base, iov->iov_len);
      assert(r == 0);
      off += iov->iov_len;
   }

   mongoc_array_destroy(&ar);
   bson_free(data);
}


static void
test_mongoc_rpc_delete (void)
{
   mongoc_rpc_t rpc;
   bson_t sel;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&sel);

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 1234;
   rpc.delete.response_to = -1;
   rpc.delete.zero = 0;
   rpc.delete.op_code = MONGOC_OPCODE_DELETE;
   snprintf(rpc.delete.collection, sizeof rpc.delete.collection, "%s", "test.test");
   rpc.delete.flags = MONGOC_DELETE_SINGLE_REMOVE;
   rpc.delete.selector = &sel;

   assert_rpc_equal("delete1.dat", &rpc);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/rpc/delete", test_mongoc_rpc_delete);

   return 0;
}

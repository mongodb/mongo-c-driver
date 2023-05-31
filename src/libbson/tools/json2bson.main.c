#include <bson/bson.h>

#include "./common.h"

int
main (int argc, char **argv)
{
   if (argc != 1) {
      fputs ("Usage:\n"
             "  Pipe a JSON document through standard input, and this program\n"
             "  will write bson data to standard output.\n",
             stderr);
      return 1;
   }

   int retcode = 0;

   read_result read = read_stream (stdin);
   if (read.error) {
      fprintf (
         stderr, "Failed to read from stdin: %s\n", strerror (read.error));
      retcode = 2;
      goto read_fail;
   }

   bson_error_t error;
   bson_t *b = bson_new_from_json (read.data, read.len, &error);
   if (!b) {
      fprintf (stderr,
               "Failed to read JSON into BSON: %d:%d %s\n",
               error.domain,
               error.code,
               error.message);
      goto from_json_fail;
   }

   const uint8_t *bdata = bson_get_data (b);
   for (size_t remain = b->len; remain;) {
      size_t nwritten = fwrite (bdata, 1, remain, stdout);
      remain -= nwritten;
      bdata += nwritten;
   }

from_json_fail:
   bson_destroy (b);
   free (read.data);
read_fail:
   return retcode;
}

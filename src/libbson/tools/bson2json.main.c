#include <bson/bson.h>

#include "./common.h"


int
main (int argc, char **argv)
{
   if (argc != 1) {
      fputs ("Usage:\n"
             "  Pipe a BSON document through standard input, and this program\n"
             "  will write JSON data to standard output.\n",
             stderr);
      return 1;
   }

   int retcode = 0;

   read_result read = read_stream (stdin);
   if (read.error) {
      fprintf (stderr, "Failed to read from stdin: %s", strerror (read.error));
      retcode = 2;
      goto read_fail;
   }

   bson_t b;
   if (!bson_init_static (&b, read.data, read.len)) {
      fputs ("Failed to read BSON: Invalid header\n", stderr);
      retcode = 3;
      goto bson_init_fail;
   }

   size_t len;
   char *json = bson_as_canonical_extended_json (&b, &len);
   if (!json) {
      fputs ("Failed to create JSON data\n", stderr);
      retcode = 4;
      goto json_fail;
   }

   const char *jptr = json;
   for (size_t remain = len; remain;) {
      size_t nwritten = fwrite (jptr, 1, remain, stdout);
      remain -= nwritten;
      jptr += nwritten;
   }

json_fail:
   bson_free (json);
bson_init_fail:
   free (read.data);
read_fail:
   return retcode;
}

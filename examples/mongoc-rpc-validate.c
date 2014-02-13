/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*
 * This example can only be used internally to the library as it uses
 * private features that are not exported in the public ABI. It does,
 * however, illustrate some of the internals of the system.
 */


#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-array-private.h>
#include <mongoc-rpc-private.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>


static void
validate (const char *name,
          mongoc_fd_t   fd)
{
   uint8_t *buf;
   mongoc_rpc_t rpc;
   int32_t len;
   struct stat st;

   if (mongoc_fstat (fd, &st) != 0) {
      fprintf (stderr, "%s: Failed to fstat.\n", name);
      return;
   }

   if (st.st_size > (100 * 1024 * 1024)) {
      fprintf (stderr, "%s: unreasonable message size\n", name);
      return;
   }

   buf = malloc (st.st_size);
   if (buf == NULL) {
      fprintf (stderr, "%s: Failed to malloc %d bytes.\n",
               name, (int)st.st_size);
      return;
   }

   if (st.st_size != mongoc_read (fd, buf, st.st_size)) {
      fprintf (stderr, "%s: Failed to read %d bytes into buffer.\n",
               name, (int)st.st_size);
      goto cleanup;
   }

   memcpy (&len, buf, 4);
   len = BSON_UINT32_FROM_LE (len);
   if (len != st.st_size) {
      fprintf (stderr, "%s is invalid. Invalid Length.\n", name);
      goto cleanup;
   }

   if (!_mongoc_rpc_scatter (&rpc, buf, st.st_size)) {
      fprintf (stderr, "%s is invalid. Invalid Format.\n", name);
      goto cleanup;
   }

   fprintf (stdout, "%s is valid.\n", name);

cleanup:
   free (buf);
}


int
main (int   argc,
      char *argv[])
{
   mongoc_fd_t fd;
   int i;

   if (argc < 2) {
      fprintf (stderr, "usage: %s FILE...\n", argv[0]);
      return EXIT_FAILURE;
   }

   mongoc_init();

   for (i = 1; i < argc; i++) {
      fd = mongoc_open (argv[i], O_RDONLY);
      if (! mongoc_fd_is_valid(fd)) {
         fprintf (stderr, "Failed to open \"%s\"\n", argv[i]);
         continue;
      }
      validate (argv[i], fd);
      mongoc_close (fd);
   }

   return EXIT_SUCCESS;
}

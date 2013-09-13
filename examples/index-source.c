/*
 * Copyright 2013 10gen Inc.
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
 * This is an example program that will use libclang to index and search
 * for types within a program. It will store the information into MongoDB
 * using libmongoc and query it as well.
 *
 * See --help for more information.
 */


#include <clang-c/Index.h>
#include <ctype.h>
#include <mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


static char **gCompilerArgs;
static int    gCompilerArgsCount;


static bson_bool_t
is_source (const char *filename)
{
   int len;

   len = strlen(filename);

   if (!strcmp(".c", filename + len - 2) ||
       !strcmp(".h", filename + len - 2) ||
       !strcmp(".cc", filename + len  - 3) ||
       !strcmp(".hh", filename + len  - 3) ||
       !strcmp(".cpp", filename + len  - 4) ||
       !strcmp(".hpp", filename + len  - 4)) {
      return TRUE;
   }

   fprintf(stderr, "Unknown file type: %s\n", filename);

   return FALSE;
}


static size_t
get_file_size (const char *filename)
{
   struct stat st;

   if (!stat(filename, &st)) {
      return st.st_size;
   }

   return 0;
}


static void
append_location_doc (bson_t           *doc,
                     const char       *key,
                     CXSourceLocation  location)
{
   unsigned line;
   unsigned column;
   unsigned offset;
   bson_t child;
   CXFile file;

   bson_append_document_begin(doc, key, -1, &child);

   clang_getExpansionLocation(location, &file, &line, &column, &offset);

   bson_append_int32(&child, "line", 4, line);
   bson_append_int32(&child, "column", 6, column);
   bson_append_int32(&child, "offset", 6, offset);

   bson_append_document_end(doc, &child);
}


static void
index_part (mongoc_collection_t *collection,
            CXTranslationUnit    unit,
            const char          *filename,
            CXCursor             cursor,
            CXSourceRange        range)
{
   CXSourceLocation begin;
   CXSourceLocation end;
   bson_error_t error;
   const char *cstr;
   CXString xstr;
   bson_t b;
   bson_t ar;

   begin = clang_getRangeStart(range);
   end = clang_getRangeEnd(range);

   bson_init(&b);
   bson_append_utf8(&b, "filename", 8, filename, -1);

   xstr = clang_getCursorSpelling(cursor);
   cstr = clang_getCString(xstr);
   if (!cstr || !*cstr) {
      bson_destroy(&b);
      return;
   }
   bson_append_utf8(&b, "spelling", 8, cstr, -1);
   clang_disposeString(xstr);

   bson_append_array_begin(&b, "range", 5, &ar);
   append_location_doc(&ar, "0", begin);
   append_location_doc(&ar, "1", end);
   bson_append_array_end(&b, &ar);

   if (!mongoc_collection_insert(collection,
                                 MONGOC_INSERT_NONE,
                                 &b,
                                 NULL,
                                 &error)) {
      fprintf(stderr, "Error inserting: %s\n", error.message);
   }

   bson_destroy(&b);
}


static void
index_source (mongoc_collection_t *collection,
              const char          *filename)
{
   CXTranslationUnit unit = NULL;
   CXSourceLocation begin;
   CXSourceLocation end;
   CXSourceRange range = {{ 0 }};
   unsigned n_tokens = 0;
   unsigned i;
   CXToken *tokens = NULL;
   CXIndex index = NULL;
   CXFile file;

   fprintf(stderr, "Indexing %s:\n", filename);

   index = clang_createIndex(0, 0);
   unit = clang_createTranslationUnitFromSourceFile(index,
                                                    filename,
                                                    gCompilerArgsCount,
                                                    (const char * const *)gCompilerArgs,
                                                    0,
                                                    NULL);
   if (!unit) {
      fprintf(stderr, "  Failed to compile.\n");
      goto cleanup;
   }

   file = clang_getFile(unit, filename);
   begin = clang_getLocationForOffset(unit, file, 0);
   end = clang_getLocationForOffset(unit, file, get_file_size(filename));
   range = clang_getRange(begin, end);

   clang_tokenize(unit, range, &tokens, &n_tokens);
   if (!tokens) {
      goto cleanup;
   }

   fprintf(stderr, "  Successfully tokenized %s\n", filename);

   for (i = 0; i < n_tokens; i++) {
      CXSourceLocation loc;
      CXCursor cursor;

      if (clang_getTokenKind(tokens[i]) != CXToken_Identifier) {
         continue;
      }

      loc = clang_getTokenLocation(unit, tokens[i]);
      cursor = clang_getCursor(unit, loc);

      index_part(collection, unit, filename, cursor, range);
   }

cleanup:
   if (index) {
      clang_disposeIndex(index);
   }
   if (tokens) {
      clang_disposeTokens(unit, tokens, n_tokens);
   }
   if (unit) {
      clang_disposeTranslationUnit(unit);
   }
}


static void
usage (const char *prgname)
{
   fprintf(stderr, "usage: %s [OPTIONS] filenames...\n", prgname);
   fprintf(stderr, "\n");
   fprintf(stderr, "Options\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "  -H URI_STRING     The uri string to MongoDB.\n");
   fprintf(stderr, "\n");
}


int
main (int   argc,
      char *argv[])
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   const char *uristr = NULL;
   int c;
   int i;

   opterr = 0;

   while ((c = getopt(argc, argv, "H:")) != -1) {
      switch (c) {
      case 'H':
         uristr = optarg;
         break;
      case '?':
         usage(argv[0]);
         return EXIT_SUCCESS;
      default:
         fprintf(stderr, "Unknown argument: %c\n", (char)c);
         return EXIT_FAILURE;
      }
   }

   /*
    * Create our lazy connection to MongoDB.
    */
   uri = mongoc_uri_new(uristr);
   client = mongoc_client_new_from_uri(uri);
   collection = mongoc_client_get_collection(client, "source", "symbols");

   /*
    * Try to find our compiler flags, which occur after --
    */
   for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--")) {
         gCompilerArgsCount = argc - i - 1;
         gCompilerArgs = &argv[i+1];
         break;
      }
   }

   /*
    * Process the provided filenames.
    */
   for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--")) {
         break;
      }
      if ((access(argv[i], R_OK) == 0) && is_source(argv[i])) {
         index_source(collection, argv[i]);
      }
   }

   /*
    * Cleanup resources.
    */
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
   mongoc_uri_destroy(uri);

   return 0;
}

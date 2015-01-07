/*
 * Copyright 2015 MongoDB, Inc.
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

#include "json-test.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

/*
 *-----------------------------------------------------------------------
 *
 * assemble_path --
 *
 *       Given a parent directory and filename, compile a full path to
 *       the child file.
 *
 *-----------------------------------------------------------------------
 */
void
assemble_path (const char *parent_path,
               const char *child_name,
               char       *dst /* OUT */)
{
   int path_len = (int)strlen(parent_path);
   int name_len = (int)strlen(child_name);

   assert(path_len + name_len + 1 < MAX_NAME_LENGTH);

   memset(dst, '\0', MAX_NAME_LENGTH * sizeof(char));
   strncat(dst, parent_path, path_len);
   strncat(dst, "/", 1);
   strncat(dst, child_name, name_len);
}

/*
 *-----------------------------------------------------------------------
 *
 * collect_tests_from_dir --
 *
 *       Recursively search the directory at @dir_path for files with
 *       '.json' in their filenames. Append all found file paths to
 *       @paths, and return the number of files found.
 *
 *-----------------------------------------------------------------------
 */
int
collect_tests_from_dir (char (*paths)[MAX_NAME_LENGTH] /* OUT */,
                        const char *dir_path,
                        int paths_index,
                        int max_paths)
{
   struct dirent *entry;
   char child_path[MAX_NAME_LENGTH];
   DIR *dir;

   dir = opendir(dir_path);

   while (dir) {

      assert(paths_index < max_paths);

      entry = readdir(dir);
      if (!entry) {
         /* out of entries */
         break;
      }

      if (entry->d_type & DT_DIR) {
         /* recursively call on child directories */
         if (strcmp (entry->d_name, "..") != 0 &&
             strcmp (entry->d_name, ".") != 0) {

            assemble_path(dir_path, entry->d_name, child_path);
            paths_index = collect_tests_from_dir(paths, child_path, paths_index, max_paths);
         }
      } else if (strstr(entry->d_name, ".json")) {
         /* if this is a JSON test, collect its path */
         assemble_path(dir_path, entry->d_name, paths[paths_index++]);
      }
   }

   if (dir) {
      closedir(dir);
   }
   return paths_index;
}

/*
 *-----------------------------------------------------------------------
 *
 * get_bson_from_json_file --
 *
 *        Open the file at @filename and store its contents in a
 *        bson_t. This function assumes that @filename contains a
 *        single JSON object.
 *
 *        NOTE: caller owns returned bson_t and must free it.
 *
 *-----------------------------------------------------------------------
 */
bson_t *
get_bson_from_json_file(char *filename)
{
   FILE *file;
   long length;
   bson_t *data;
   bson_error_t error;
   const char *buffer;

   file = fopen(filename, "r");
   if (!file) {
      return NULL;
   }

   /* get file length */
   fseek(file, 0, SEEK_END);
   length = ftell(file);
   fseek(file, 0, SEEK_SET);
   if (length < 1) {
      return NULL;
   }

   /* read entire file into buffer */
   buffer = bson_malloc0(length);
   fread((void *)buffer, 1, length, file);
   fclose(file);
   if (!buffer) {
      return NULL;
   }

   /* convert to bson */
   data = bson_new_from_json((const uint8_t*)buffer, length, &error);
   bson_free((void *)buffer);
   if (!data) {
      return NULL;
   }
   return data;
}

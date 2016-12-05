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


#include <string.h>

#include "mongoc-util-private.h"
#include "mongoc-client.h"
#include "mongoc-trace-private.h"


char *
_mongoc_hex_md5 (const char *input)
{
   uint8_t digest[16];
   bson_md5_t md5;
   char digest_str[33];
   int i;

   bson_md5_init(&md5);
   bson_md5_append(&md5, (const uint8_t *)input, (uint32_t)strlen(input));
   bson_md5_finish(&md5, digest);

   for (i = 0; i < sizeof digest; i++) {
      bson_snprintf(&digest_str[i*2], 3, "%02x", digest[i]);
   }
   digest_str[sizeof digest_str - 1] = '\0';

   return bson_strdup(digest_str);
}


void
_mongoc_usleep (int64_t usec)
{
#ifdef _WIN32
   LARGE_INTEGER ft;
   HANDLE timer;

   BSON_ASSERT (usec >= 0);

   ft.QuadPart = -(10 * usec);
   timer = CreateWaitableTimer(NULL, true, NULL);
   SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
   WaitForSingleObject(timer, INFINITE);
   CloseHandle(timer);
#else
   BSON_ASSERT (usec >= 0);
   usleep ((useconds_t) usec);
#endif
}


const char *
_mongoc_get_command_name (const bson_t *command)
{
   bson_iter_t iter;
   const char *name;
   bson_iter_t child;
   const char *wrapper_name = NULL;

   BSON_ASSERT (command);

   if (!bson_iter_init (&iter, command) ||
       !bson_iter_next (&iter)) {
      return NULL;
   }

   name = bson_iter_key (&iter);

   /* wrapped in "$query" or "query"?
    *
    *   {$query: {count: "collection"}, $readPreference: {...}}
    */
   if (name[0] == '$') {
      wrapper_name = "$query";
   } else if (!strcmp (name, "query")) {
      wrapper_name = "query";
   }

   if (wrapper_name &&
       bson_iter_init_find (&iter, command, wrapper_name) &&
       BSON_ITER_HOLDS_DOCUMENT (&iter) &&
       bson_iter_recurse (&iter, &child) &&
       bson_iter_next (&child)) {

      name = bson_iter_key (&child);
   }

   return name;
}


void
_mongoc_get_db_name (const char *ns,
                     char *db /* OUT */)
{
   size_t dblen;
   const char *dot;

   BSON_ASSERT (ns);

   dot = strstr (ns, ".");

   if (dot) {
      dblen = BSON_MIN (dot - ns + 1, MONGOC_NAMESPACE_MAX);
      bson_strncpy (db, ns, dblen);
   } else {
      bson_strncpy (db, ns, MONGOC_NAMESPACE_MAX);
   }
}

void
_mongoc_bson_destroy_if_set (bson_t *bson)
{
   if (bson) {
      bson_destroy (bson);
   }
}

size_t
_mongoc_strlen_or_zero (const char *s)
{
   return s ? strlen (s) : 0;
}


/* Get "serverId" from opts. Sets *server_id to the serverId from "opts" or 0
 * if absent. On error, fills out *error with domain and code and return false.
 */
bool
_mongoc_get_server_id_from_opts (const bson_t          *opts,
                                 mongoc_error_domain_t  domain,
                                 mongoc_error_code_t    code,
                                 uint32_t              *server_id,
                                 bson_error_t          *error)
{
   bson_iter_t iter;

   ENTRY;

   BSON_ASSERT (server_id);

   *server_id = 0;

   if (!opts || !bson_iter_init_find (&iter, opts, "serverId")) {
      RETURN (true);
   }

   if (!BSON_ITER_HOLDS_INT32 (&iter) && !BSON_ITER_HOLDS_INT64 (&iter)) {
      bson_set_error (error, domain, code,
                      "The serverId option must be an integer");
      RETURN (false);
   }

   if (bson_iter_as_int64 (&iter) <= 0) {
      bson_set_error (error, domain, code,
                      "The serverId option must be >= 1");
      RETURN (false);
   }

   *server_id = (uint32_t) bson_iter_as_int64 (&iter);

   RETURN (true);
}

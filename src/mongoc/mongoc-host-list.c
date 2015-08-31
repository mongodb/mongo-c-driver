/*
 * Copyright 2015 MongoDB Inc.
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

#include "mongoc-host-list.h"
#include "mongoc-util-private.h"

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_host_list_new --
 *
 *       Empty new host list.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
mongoc_host_list_t *
mongoc_host_list_new (void)
{
   return (mongoc_host_list_t *) bson_malloc0 (sizeof (mongoc_host_list_t));
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_host_list_equal --
 *
 *       Check two hosts have the same domain (case-insensitive), port,
 *       and address family.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
bool
mongoc_host_list_equal (const mongoc_host_list_t *host_a,
                        const mongoc_host_list_t *host_b)
{
   return (!strcasecmp (host_a->host_and_port, host_b->host_and_port)
           && host_a->family == host_b->family);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_host_list_find --
 *
 *       Search for an equal mongoc_host_list_t in a list of them.
 *
 * Returns:
 *       Pointer to an entry in "list", or NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
const mongoc_host_list_t *
mongoc_host_list_find (const mongoc_host_list_t *list,
                       const mongoc_host_list_t *needle)
{
   while (list) {
      if (mongoc_host_list_equal (list, needle)) {
         return list;
      }

      list = list->next;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_host_list_count --
 *
 *       Return number of items in the host list.
 *
 * Returns:
 *       0 or greater.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
size_t
mongoc_host_list_count (const mongoc_host_list_t *list)
{
   size_t count = 0;

   while (list) {
      count++;
      list = list->next;
   }

   return count;
}


/*
 *--------------------------------------------------------------------------
 *
 * host_list_copy --
 *
 *       Make a copy of "host" with next set to "next".
 *
 * Returns:
 *       New copy.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
mongoc_host_list_t *
mongoc_host_list_copy (const mongoc_host_list_t *host,
                       mongoc_host_list_t *next)
{
   mongoc_host_list_t *copy;

   copy = mongoc_host_list_new ();
   memcpy ((void *) copy, (void *) host, sizeof (mongoc_host_list_t));
   copy->next = next;

   return copy;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_host_list_destroy_all --
 *
 *       Destroy whole linked list of hosts.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_host_list_destroy_all (mongoc_host_list_t *host)
{
   mongoc_host_list_t *tmp;

   while (host) {
      tmp = host->next;
      bson_free (host);
      host = tmp;
   }
}

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

#include "mongoc-host-list-private.h"
/* strcasecmp on windows */
#include "mongoc-util-private.h"


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_host_list_equal --
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
_mongoc_host_list_equal (const mongoc_host_list_t *host_a,
                         const mongoc_host_list_t *host_b)
{
   return (!strcasecmp (host_a->host_and_port, host_b->host_and_port) &&
           host_a->family == host_b->family);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_host_list_destroy_all --
 *
 *       Destroy whole linked list of hosts.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_host_list_destroy_all (mongoc_host_list_t *host)
{
   mongoc_host_list_t *tmp;

   while (host) {
      tmp = host->next;
      bson_free (host);
      host = tmp;
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_host_list_from_string --
 *
 *       Populate a mongoc_host_list_t from a fully qualified address
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_host_list_from_string (mongoc_host_list_t *link_, const char *address)
{
   char *sport;
   uint16_t port;

   mongoc_lowercase (address, link_->host);
   sport = strrchr (address, ':');
   if (sport) {
      bson_snprintf (
         link_->host_and_port, sizeof link_->host_and_port, "%s", address);
      link_->host[strlen (link_->host) - strlen (sport)] = '\0';

      mongoc_parse_port (&port, sport + 1);
      link_->port = port;
   } else {
      link_->port = MONGOC_DEFAULT_PORT;
   }

   if (*address == '[' && strchr (address, ']')) {
      link_->family = AF_INET6;
   } else if (strchr (address, '/') && strstr (address, ".sock")) {
      link_->family = AF_UNIX;
   } else {
      link_->family = AF_INET;
   }
   link_->next = NULL;
   return true;
}

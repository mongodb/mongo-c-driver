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


#include "mongoc-client.h"
#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-event-private.h"
#include "mongoc-list-private.h"
#include "mongoc-queue-private.h"


struct _mongoc_client_t
{
   bson_uint32_t     request_id;
   mongoc_list_t    *conns;
   mongoc_uri_t     *uri;
   mongoc_cluster_t  cluster;
};


bson_bool_t
mongoc_client_send (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_error_t    *error)
{
#if 0
   bson_bool_t ret = FALSE;

   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(event, FALSE);

   event->any.opcode = event->type;
   event->any.response_to = -1;
   event->any.request_id = ++client->request_id;

   ret = mongoc_event_write(event, client->outfd, error);

   return ret;
#else
   return FALSE;
#endif
}


mongoc_client_t *
mongoc_client_new (const char *uri_string)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   if (!(uri = mongoc_uri_new(uri_string))) {
      return NULL;
   }

   client = bson_malloc0(sizeof *client);
   client->uri = uri;
   client->request_id = rand();

   return client;
}


mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri)
{
   const char *uristr;

   bson_return_val_if_fail(uri, NULL);

   uristr = mongoc_uri_get_string(uri);
   return mongoc_client_new(uristr);
}


void
mongoc_client_destroy (mongoc_client_t *client)
{
   /*
    * TODO: Implement destruction.
    */
   mongoc_uri_destroy(client->uri);
   bson_free(client);
}


const mongoc_uri_t *
mongoc_client_get_uri (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);
   return client->uri;
}

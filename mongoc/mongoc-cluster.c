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


#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"
#include "mongoc-log.h"


void
mongoc_cluster_init (mongoc_cluster_t   *cluster,
                     const mongoc_uri_t *uri,
                     void               *client)
{
   const mongoc_host_list_t *hosts;
   const bson_t *b;
   bson_iter_t iter;

   bson_return_if_fail(cluster);
   bson_return_if_fail(uri);

   memset(cluster, 0, sizeof *cluster);

   b = mongoc_uri_get_options(uri);
   hosts = mongoc_uri_get_hosts(uri);

   if (bson_iter_init_find_case(&iter, b, "replicaSet")) {
      cluster->mode = MONGOC_CLUSTER_REPLICA_SET;
   } else if (hosts->next) {
      cluster->mode = MONGOC_CLUSTER_SHARDED_CLUSTER;
   } else {
      cluster->mode = MONGOC_CLUSTER_DIRECT;
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
}


void
mongoc_cluster_destroy (mongoc_cluster_t *cluster)
{
   bson_return_if_fail(cluster);

   /*
    * TODO: release resources.
    */
}


static void
mongoc_cluster_ensure_stream_for (mongoc_cluster_t *cluster,
                                  const char       *host_and_port)
{
   mongoc_cluster_node_t *node;
   unsigned i;

   bson_return_if_fail(cluster);
   bson_return_if_fail(host_and_port);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      node = &cluster->nodes[i];
      if (!strcasecmp(host_and_port, node->host.host_and_port)) {
         if (!node->stream) {
            /*
             * TODO: Establish stream to host_and_port.
             */
         }
      }
   }
}


/**
 * mongoc_cluster_prepare_replica_set:
 * @cluster: A mongoc_cluster_t.
 *
 * Discover our replicaSet information from the first node that matches
 * our configured information.
 */
void
mongoc_cluster_prepare_replica_set (mongoc_cluster_t *cluster)
{
   const mongoc_host_list_t *iter;
   mongoc_stream_t *stream = NULL;
   bson_error_t error = { 0 };
   bson_iter_t bi;
   bson_iter_t ar;
   const char *setName = NULL;
   const char *hostport;
   bson_t *b;

   bson_return_if_fail(cluster);
   bson_return_if_fail(cluster->mode == MONGOC_CLUSTER_REPLICA_SET);

   bson_iter_init(&bi, mongoc_uri_get_options(cluster->uri));
   if (bson_iter_find_case(&bi, "setName") && BSON_ITER_HOLDS_UTF8(&bi)) {
      setName = bson_iter_utf8(&bi, NULL);
   }

   iter = mongoc_uri_get_hosts(cluster->uri);
   for (; iter; iter = iter->next) {
      stream = mongoc_client_create_stream(cluster->client, iter, &error);
      if (!stream) {
         MONGOC_WARNING("Failed to connect to %s", error.message);
         bson_error_destroy(&error);
         continue;
      }

      /*
       * Execute runCommand("ismaster") to get server information.
       */
      if (!(b = mongoc_stream_ismaster(stream, &error))) {
         goto skip;
      }

      /*
       * Make sure a string setName field exists.
       */
      if (!bson_iter_init_find_case(&bi, b, "setName") ||
          !BSON_ITER_HOLDS_UTF8(&bi)) {
         goto skip;
      }

      /*
       * Make sure this node is part of our desired replicaSet.
       */
      if (setName && *setName &&
          !!strcmp(setName, bson_iter_utf8(&bi, NULL))) {
         goto skip;
      }

      /*
       * Check to see if this node is a primary or secondary so that we can
       * trust its information as authoritive about the other hosts.
       */
      if ((!bson_iter_init_find_case(&bi, b, "isMaster") ||
           !BSON_ITER_HOLDS_BOOL(&bi) ||
           !bson_iter_bool(&bi)) &&
          (!bson_iter_init_find_case(&bi, b, "secondary") ||
           !BSON_ITER_HOLDS_BOOL(&bi) ||
           !bson_iter_bool(&bi))) {
         goto skip;
      }

      /*
       * Make sure we are not talking to a mongos instance.
       */
      if (bson_iter_init_find_case(&bi, b, "msg") &&
          BSON_ITER_HOLDS_UTF8(&bi) &&
          !strcasecmp("isdbgrid", bson_iter_utf8(&bi, NULL))) {
         goto skip;
      }

      /*
       * Track the maxMessageSizeBytes.
       */
      if (bson_iter_init_find_case(&bi, b, "maxMessageSizeBytes")) {
         cluster->max_msg_size = bson_iter_int32(&bi);
      }

      /*
       * Track the maxBsonObjectSize.
       */
      if (bson_iter_init_find_case(&bi, b, "maxBsonObjectSize")) {
         cluster->max_bson_size = bson_iter_int32(&bi);
      }

      /*
       * We can trust the "hosts" field for the list of replicaSet members.
       * We can connect to each of them.
       */
      if (bson_iter_init_find_case(&bi, b, "hosts") &&
          bson_iter_recurse(&bi, &ar)) {
         while (bson_iter_next(&ar)) {
            if (BSON_ITER_HOLDS_UTF8(&ar)) {
               hostport = bson_iter_utf8(&ar, NULL);
               mongoc_cluster_ensure_stream_for(cluster, hostport);
            }
         }
      }

      bson_destroy(b);
      break;

skip:
      mongoc_stream_close(stream);
      mongoc_stream_destroy(stream);
      stream = NULL;
      if (b) {
         bson_destroy(b);
      }
   }

   if (!stream) {
      MONGOC_WARNING("No healthy replicaSet members could be found.");
   }
}

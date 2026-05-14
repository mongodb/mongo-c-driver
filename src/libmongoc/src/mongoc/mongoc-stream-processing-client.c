/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <mongoc/mongoc-stream-processing-client-private.h>
#include <mongoc/mongoc-stream-processor-private.h>

#include <mongoc/mongoc-client.h>
#include <mongoc/mongoc-error.h>
#include <mongoc/mongoc-host-list.h>
#include <mongoc/mongoc-uri.h>

#include <bson/bson.h>

#include <string.h>

#define ASP_HOST_PREFIX "atlas-stream-"
#define ASP_HOST_SUFFIX ".a.query.mongodb.net"

bool
_mongoc_is_asp_workspace_host (const char *host)
{
   size_t host_len;
   size_t suffix_len;

   if (!host) {
      return false;
   }
   if (strncmp (host, ASP_HOST_PREFIX, sizeof (ASP_HOST_PREFIX) - 1) == 0) {
      return true;
   }
   host_len = strlen (host);
   suffix_len = sizeof (ASP_HOST_SUFFIX) - 1;
   if (host_len >= suffix_len &&
       strcmp (host + host_len - suffix_len, ASP_HOST_SUFFIX) == 0) {
      return true;
   }
   return false;
}

mongoc_stream_processing_client_t *
mongoc_stream_processing_client_new_from_uri (const mongoc_uri_t *uri, bson_error_t *error)
{
   mongoc_uri_t *uri_copy = NULL;
   mongoc_stream_processing_client_t *asp_client = NULL;
   const mongoc_host_list_t *hosts;
   bool is_workspace = false;

   BSON_ASSERT (uri);

   uri_copy = mongoc_uri_copy (uri);

   /* Detect workspace endpoint and enforce TLS */
   hosts = mongoc_uri_get_hosts (uri_copy);
   while (hosts) {
      if (_mongoc_is_asp_workspace_host (hosts->host)) {
         is_workspace = true;
         break;
      }
      hosts = hosts->next;
   }

   if (!is_workspace) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "URI does not point to an Atlas Stream Processing workspace "
                      "(hostname must begin with \"atlas-stream-\" or end with "
                      "\".a.query.mongodb.net\")");
      mongoc_uri_destroy (uri_copy);
      return NULL;
   }

   /* TLS is required; force it on and reject explicit tls=false */
   if (!mongoc_uri_get_tls (uri_copy)) {
      if (!mongoc_uri_set_option_as_bool (uri_copy, MONGOC_URI_TLS, true)) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Failed to enable TLS for Atlas Stream Processing workspace connection");
         mongoc_uri_destroy (uri_copy);
         return NULL;
      }
   }

   /* authSource must be admin */
   if (!mongoc_uri_get_auth_source (uri_copy) ||
       strcmp (mongoc_uri_get_auth_source (uri_copy), "admin") != 0) {
      mongoc_uri_set_auth_source (uri_copy, "admin");
   }

   asp_client = bson_malloc0 (sizeof *asp_client);
   asp_client->client = mongoc_client_new_from_uri (uri_copy);
   mongoc_uri_destroy (uri_copy);

   if (!asp_client->client) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Failed to create underlying MongoClient for Atlas Stream Processing workspace");
      bson_free (asp_client);
      return NULL;
   }

   asp_client->sps = bson_malloc0 (sizeof *asp_client->sps);
   asp_client->sps->asp_client = asp_client;

   return asp_client;
}

void
mongoc_stream_processing_client_destroy (mongoc_stream_processing_client_t *client)
{
   if (!client) {
      return;
   }
   bson_free (client->sps);
   mongoc_client_destroy (client->client);
   bson_free (client);
}

mongoc_stream_processors_t *
mongoc_stream_processing_client_get_stream_processors (mongoc_stream_processing_client_t *client)
{
   BSON_ASSERT (client);
   return client->sps;
}

/* ---------------------------------------------------------------------------
 * Options lifecycle
 * ---------------------------------------------------------------------------*/

mongoc_create_stream_processor_opts_t *
mongoc_create_stream_processor_opts_new (void)
{
   mongoc_create_stream_processor_opts_t *opts = bson_malloc0 (sizeof *opts);
   opts->failover = -1;
   return opts;
}

void
mongoc_create_stream_processor_opts_destroy (mongoc_create_stream_processor_opts_t *opts)
{
   if (!opts) {
      return;
   }
   if (opts->dlq) {
      bson_destroy (opts->dlq);
   }
   bson_free (opts->stream_meta_field_name);
   bson_free (opts->tier);
   bson_free (opts);
}

mongoc_failover_opts_t *
mongoc_failover_opts_new (const char *region)
{
   mongoc_failover_opts_t *opts;

   BSON_ASSERT (region);
   opts = bson_malloc0 (sizeof *opts);
   opts->region = bson_strdup (region);
   opts->dry_run = -1;
   return opts;
}

void
mongoc_failover_opts_destroy (mongoc_failover_opts_t *opts)
{
   if (!opts) {
      return;
   }
   bson_free (opts->region);
   bson_free (opts->mode);
   bson_free (opts);
}

mongoc_start_stream_processor_opts_t *
mongoc_start_stream_processor_opts_new (void)
{
   mongoc_start_stream_processor_opts_t *opts = bson_malloc0 (sizeof *opts);
   opts->enable_auto_scaling = -1;
   return opts;
}

void
mongoc_start_stream_processor_opts_destroy (mongoc_start_stream_processor_opts_t *opts)
{
   if (!opts) {
      return;
   }
   /* start_after is present for API completeness; never sent on the wire */
   if (opts->start_after) {
      bson_destroy (opts->start_after);
   }
   bson_free (opts->tier);
   /* failover is not owned by this struct */
   bson_free (opts);
}

mongoc_get_stream_processor_stats_opts_t *
mongoc_get_stream_processor_stats_opts_new (void)
{
   mongoc_get_stream_processor_stats_opts_t *opts = bson_malloc0 (sizeof *opts);
   opts->verbose = -1;
   return opts;
}

void
mongoc_get_stream_processor_stats_opts_destroy (mongoc_get_stream_processor_stats_opts_t *opts)
{
   bson_free (opts);
}

mongoc_get_stream_processor_samples_opts_t *
mongoc_get_stream_processor_samples_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_get_stream_processor_samples_opts_t));
}

void
mongoc_get_stream_processor_samples_opts_destroy (mongoc_get_stream_processor_samples_opts_t *opts)
{
   bson_free (opts);
}

/* ---------------------------------------------------------------------------
 * Result / info lifecycle
 * ---------------------------------------------------------------------------*/

void
mongoc_get_stream_processor_samples_result_destroy (mongoc_get_stream_processor_samples_result_t *result)
{
   uint32_t i;

   if (!result) {
      return;
   }
   for (i = 0; i < result->document_count; i++) {
      bson_destroy (result->documents[i]);
   }
   bson_free (result->documents);
   bson_free (result);
}

void
mongoc_stream_processor_info_destroy (mongoc_stream_processor_info_t *info)
{
   if (!info) {
      return;
   }
   bson_free (info->id);
   bson_free (info->name);
   bson_free (info->state);
   if (info->pipeline) {
      bson_destroy (info->pipeline);
   }
   bson_free (info->tier);
   if (info->dlq) {
      bson_destroy (info->dlq);
   }
   bson_free (info->stream_meta_field_name);
   bson_free (info->active_region);
   bson_free (info->workspace_default_region);
   bson_free (info->modified_by);
   bson_free (info->error_msg);
   bson_free (info);
}

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"
#include "mongoc-client.h"

int64_t
mongoc_oidc_callback_params_get_timeout_ms(const mongoc_oidc_callback_params_t *callback_params)
{
   return callback_params->callback_timeout_ms;
}

int64_t
mongoc_oidc_callback_params_get_version(const mongoc_oidc_callback_params_t *callback_params)
{
   return callback_params->version;
}

void
mongoc_oidc_credential_set_access_token(mongoc_oidc_credential_t *credential, char *access_token)
{
   credential->access_token = access_token;
}

void
mongoc_oidc_credential_set_expires_in_seconds(mongoc_oidc_credential_t *credential, int64_t expires_in_seconds)
{
   credential->expires_in_seconds = expires_in_seconds;
}

bool
_mongoc_cluster_auth_node_oidc (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error)
{
   bool ok = true;
   mongoc_oidc_callback_params_t params;
   mongoc_oidc_credential_t creds;

#undef MIN
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

   params.version = 1;
   /*
    * TODO: set timeout to:
    *     min(remaining connectTimeoutMS, remaining timeoutMS)
    */
   params.callback_timeout_ms = MIN (100, 200); /* placeholder */

   BSON_ASSERT (sd);
   BSON_ASSERT (error);
   BSON_ASSERT (stream);
   BSON_ASSERT (cluster);
   BSON_ASSERT (cluster->client);
   BSON_ASSERT (cluster->client->oidc_callback);

   /*
    * 1) Call callback function with params.
    */
   ok = cluster->client->oidc_callback (&params, &creds);
   if (!ok) {
      goto done;
   }

   /*
    * Store the resulting access token in the client.
    */
   cluster->client->oidc_credential->access_token = bson_strdup (creds.access_token);
   cluster->client->oidc_credential->expires_in_seconds = creds.expires_in_seconds;

done:
   return ok;

#undef MIN
}

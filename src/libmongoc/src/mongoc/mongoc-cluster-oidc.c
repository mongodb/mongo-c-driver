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

static bool
_oidc_get_token (mongoc_client_t *client)
{
#undef MONGOC_MIN
#define MONGOC_MIN(A, B) (((A) < (B)) ? (A) : (B))

   bool ok = true;
   mongoc_oidc_callback_params_t params;
   mongoc_oidc_credential_t creds;
   char *prev_token = NULL;

   params.version = 1;
   /*
    * TODO: set timeout to:
    *     min(remaining connectTimeoutMS, remaining timeoutMS)
    */
   params.callback_timeout_ms = MONGOC_MIN (100, 200); /* placeholder */

   BSON_ASSERT (client);
   BSON_ASSERT (client->oidc_callback);

   /*
    * 1) Call callback function with params.
    */
   ok = client->oidc_callback (&params, &creds);
   if (!ok) {
      goto done;
   }

   /*
    * 2) Zero out and free the previous token
    */
   prev_token = client->oidc_credential->access_token;
   if (prev_token) {
      bson_zero_free (prev_token, strlen (prev_token));
   }

   /*
    * 3) Store the resulting access token in the client.
    */
   client->oidc_credential->access_token = bson_strdup (creds.access_token);
   client->oidc_credential->expires_in_seconds = creds.expires_in_seconds;

done:
   return ok;

#undef MONGOC_MIN
}

static bool
_oidc_sasl_one_step_conversation (mongoc_client_t *client)
{
    bool ok = true;
    bson_t jwt_step_request = BSON_INITIALIZER;
    bson_t client_command = BSON_INITIALIZER;

    bson_append_utf8 (&jwt_step_request,
                      "jwt",
                      -1,
                      client->oidc_credential->access_token,
                      -1);

    BCON_APPEND (&client_command,
                 "saslStart",
                 BCON_INT32 (1),
                 "mechanism",
                 "MONGODB-AWS",
                 "payload",
                 BCON_BIN (BSON_SUBTYPE_BINARY, bson_get_data (&jwt_step_request), jwt_step_request.len));

    bson_destroy (&jwt_step_request);
    bson_destroy (&client_command);
    return ok;
}

bool
_mongoc_cluster_auth_node_oidc (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error)
{
   BSON_ASSERT (sd);
   BSON_ASSERT (error);
   BSON_ASSERT (stream);
   BSON_ASSERT (cluster);

   bool ok = true;

   /*
    * TODO:
    * - token caching
    * - token refresh
    * - token expiration
    */

   ok = _oidc_get_token (cluster->client);
   if (!ok) {
      goto done;
   }

   ok = _oidc_sasl_one_step_conversation (cluster->client);
   if (!ok) {
      goto done;
   }

done:
   return ok;
}

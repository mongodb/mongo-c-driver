#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mongoc-cluster-private.h"
#include "mongoc-cluster-sasl-private.h"
#include "mongoc-client-private.h"
#include "mongoc-client.h"
#include "mongoc-util-private.h"

int64_t
mongoc_oidc_callback_params_get_timeout_ms (const mongoc_oidc_callback_params_t *callback_params)
{
   return callback_params->callback_timeout_ms;
}

int64_t
mongoc_oidc_callback_params_get_version (const mongoc_oidc_callback_params_t *callback_params)
{
   return callback_params->version;
}

void
mongoc_oidc_credential_set_access_token (mongoc_oidc_credential_t *credential, char *access_token)
{
   credential->access_token = bson_strdup (access_token);
}

void
mongoc_oidc_credential_set_expires_in_seconds (mongoc_oidc_credential_t *credential, int64_t expires_in_seconds)
{
   credential->expires_in_seconds = expires_in_seconds;
}

/* Spec:
 * "Drivers MUST ensure that only one call to the configured provider or OIDC callback can happen at a time."
 * Presumably, this means that only a single callback GLOBALLY may be called at a time.
 * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#credential-caching
 */
static bson_mutex_t _oidc_callback_mutex;
static bson_once_t _init_oidc_callback_mutex_once_control = BSON_ONCE_INIT;

/*
 * Populate the client with the OIDC authentication token. The user MUST
 * implement a callback function which populates the mongoc_oidc_credential_t
 * object with the OIDC token and the token's timeout. The user can set the
 * callback by using the function: `mongoc_client_set_oidc_callback`.
 *
 * Spec:
 * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#one-step
 */
static bool
_oidc_set_client_token (mongoc_client_t *client, bool *is_cache, bson_error_t *error)
{
#undef MONGOC_MIN
#define MONGOC_MIN(A, B) (((A) < (B)) ? (A) : (B))

   bool ok = true;
   mongoc_oidc_callback_params_t params;
   mongoc_oidc_credential_t creds;

   BSON_ASSERT (client);
   BSON_ASSERT (client->topology);

   if (!client->topology->oidc_callback) {
      MONGOC_ERROR ("An OIDC callback function MUST be set in order to use MONGODB-OIDC as an authMechanism. "
                    "Use mongoc_client_set_oidc_callback to set the callback for single threaded clients, "
                    "or use mongoc_client_pool_set_oidc_callback for client pools.");
      ok = false;
      goto unlock_oidc_mutex;
   }

   /* Check cache if we already have a token.
    * Otherwise use the user's callback to get a new token */
   bson_mutex_lock (&client->topology->oidc_mtx);
   if (client->topology->oidc_credential->access_token) {
      *is_cache = true;
      goto unlock_oidc_mutex;
   }

   params.version = 1;
   /*
    * TODO: set timeout to:
    *     min(remaining connectTimeoutMS, remaining timeoutMS)
    */
   params.callback_timeout_ms = MONGOC_MIN (100, client->topology->connect_timeout_msec); /* placeholder */

   bson_mutex_lock (&_oidc_callback_mutex);

   /* Call the user provided callback function with params. */
   ok = client->topology->oidc_callback (&params, &creds);
   if (!ok) {
      MONGOC_ERROR ("error from within user provided OIDC callback");
      goto unlock_callback_mutex;
   }

   /* creds.access_token is a user provided string.
    * If the user's function is successful, which is implied by a return value
    * of 'true', then this string MUST NOT be NULL. */
   BSON_ASSERT (creds.access_token);

   /* Store the resulting access token in the client. */
   client->topology->oidc_credential->access_token = creds.access_token;
   client->topology->oidc_credential->expires_in_seconds = creds.expires_in_seconds;

unlock_callback_mutex:
   bson_mutex_unlock (&_oidc_callback_mutex);
unlock_oidc_mutex:
   bson_mutex_unlock (&client->topology->oidc_mtx);
   return ok;

#undef MONGOC_MIN
}

/*
 * Authenticate with the server using the OIDC SASL One Step Conversation.
 * Before calling this function, you must first populate the client with an OIDC
 * token using the _oidc_set_client_token() function.
 *
 * Spec:
 * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#one-step
 */
static bool
_oidc_sasl_one_step_conversation (mongoc_cluster_t *cluster,
                                  mongoc_stream_t *stream,
                                  mongoc_server_description_t *sd,
                                  bson_error_t *error)
{
   bool ok = true;
   bson_t jwt_step_request = BSON_INITIALIZER;
   bson_t client_command = BSON_INITIALIZER;
   bson_t server_reply = BSON_INITIALIZER;
   bson_iter_t iter;
   int conv_id = 0;

   bson_mutex_lock (&cluster->client->topology->oidc_mtx);
   bson_append_utf8 (&jwt_step_request, "jwt", -1, cluster->client->topology->oidc_credential->access_token, -1);
   bson_mutex_unlock (&cluster->client->topology->oidc_mtx);

   char *json = bson_as_json(&jwt_step_request, NULL);
   fprintf(stderr, "JWT STEP REQUEST:\n%s\n", json);
   bson_free(json);
   BCON_APPEND (&client_command,
                "saslStart",
                BCON_INT32 (1),
                "mechanism",
                "MONGODB-OIDC",
                "payload",
                BCON_BIN (BSON_SUBTYPE_BINARY, bson_get_data (&jwt_step_request), jwt_step_request.len));

   char *s = bson_as_json (&client_command, NULL);
   bson_free (s);
   /* Send the authentication command to the server. */
   ok = _mongoc_sasl_run_command (cluster, stream, sd, &client_command, &server_reply, error);
   if (!ok) {
      /* Try to get the server response, if we can't then return a generic error */
      if (!bson_iter_init (&iter, &server_reply)) {
         goto one_step_generic_error;
      }

      /* If we find the 'errmsg', then provide it to the user in the error message */
      if (bson_iter_find (&iter, "errmsg") && BSON_ITER_HOLDS_UTF8 (&iter)) {
         const char *errmsg = bson_iter_utf8 (&iter, NULL);
         MONGOC_ERROR ("failed to run OIDC SASL one-step conversation command: server reply: %s", errmsg);
         fprintf (stderr, "ERROR: %s\n", error->message);
         goto done;
      }

   one_step_generic_error:
      MONGOC_ERROR ("failed to run OIDC SASL one-step conversation command");
      goto done;
   }

   conv_id = _mongoc_cluster_get_conversation_id (&server_reply);
   if (!conv_id) {
      ok = false;
      MONGOC_ERROR ("server reply did not contain conversationId for OIDC one-step SASL");
      goto done;
   }

done:
   bson_destroy (&jwt_step_request);
   bson_destroy (&client_command);
   bson_destroy (&server_reply);
   return ok;
}

static BSON_ONCE_FUN (_mongoc_init_oidc_callback_mutex)
{
   bson_mutex_init (&_oidc_callback_mutex);

   BSON_ONCE_RETURN;
}

bool
_mongoc_cluster_auth_node_oidc (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error)
{
   bool ok = true;
   bool first_time = true;
   bool is_cache = false;

   BSON_ASSERT (sd);
   BSON_ASSERT (error);
   BSON_ASSERT (stream);
   BSON_ASSERT (cluster);

   bson_once (&_init_oidc_callback_mutex_once_control, _mongoc_init_oidc_callback_mutex);

   /*
    * Fetch an OIDC access token using the user's callback function.
    * Store the access token in the client.
    *
    * Spec:
    * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#oidc-callback
    */
   ok = _oidc_set_client_token (cluster->client, &is_cache, error);
   if (!ok) {
      goto fail;
   }

again:
   /*
    * Connect to the server using OIDC One Step Authentication:
    *
    * Spec:
    * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#conversation-6
    */
   ok = _oidc_sasl_one_step_conversation (cluster, stream, sd, error);
   if (!ok && is_cache && first_time) {
      char *cached_token = NULL;

      bson_mutex_lock (&cluster->client->topology->oidc_mtx);
      if (cluster->client->topology->oidc_credential->access_token) {
         cached_token = bson_strdup (cluster->client->topology->oidc_credential->access_token);
      }
      bson_mutex_unlock (&cluster->client->topology->oidc_mtx);

      first_time = false;
      /* Invalidate the token cache before retrying */
      if (cached_token) {
         mongoc_client_oidc_credential_invalidate (cluster->client, cached_token);
         bson_free (cached_token);
      }

      _mongoc_usleep (100);
      goto again;
   }
   if (!ok) {
      goto fail;
   }

fail:
   return ok;
}

bool
_mongoc_cluster_oidc_reauthenticate (mongoc_cluster_t *cluster,
                                     mongoc_stream_t *stream,
                                     mongoc_server_description_t *sd,
                                     bson_error_t *error)
{
   char *cached_token = NULL;

   bson_mutex_lock (&cluster->client->topology->oidc_mtx);
   cached_token = bson_strdup (cluster->client->topology->oidc_credential->access_token);
   bson_mutex_unlock (&cluster->client->topology->oidc_mtx);

   mongoc_client_oidc_credential_invalidate (cluster->client, cached_token);
   bson_free (cached_token);
   return _mongoc_cluster_auth_node_oidc (cluster, stream, sd, error);
}

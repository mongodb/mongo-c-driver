#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <mongoc/mongoc-cluster-private.h>
#include <mongoc/mongoc-cluster-sasl-private.h>
#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-client.h>
#include <mongoc/mongoc-oidc-callback.h>
#include <mongoc/mongoc-oidc-callback-private.h>
#include <mongoc/mongoc-util-private.h>

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
 * On error, returns false and sets 'error' if provided.
 * On success, returns true and sets 'is_cache'.
 *
 * Spec:
 * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#one-step
 */
static bool
_oidc_set_client_token (mongoc_client_t *client, bool *is_cache, bson_error_t *error)
{
   BSON_ASSERT_PARAM (client);
   BSON_ASSERT_PARAM (is_cache);
   BSON_OPTIONAL_PARAM (error);

   bool ok = false;
   mongoc_oidc_callback_params_t *params = NULL;

   BSON_ASSERT (client->topology);
   const mongoc_oidc_callback_t *oidc_callback = client->topology->oidc_callback;
   if (!oidc_callback) {
      MONGOC_ERROR ("An OIDC callback function MUST be set in order to use MONGODB-OIDC as an authMechanism. "
                    "Use mongoc_client_set_oidc_callback to set the callback for single threaded clients, "
                    "or use mongoc_client_pool_set_oidc_callback for client pools.");
      goto done;
   }

   /* Check cache if we already have a token.
    * Otherwise use the user's callback to get a new token */
   bson_mutex_lock (&client->topology->oidc_mtx);
   if (client->topology->oidc_credential) {
      *is_cache = true;
      goto unlock_oidc_mutex;
   }

   /*
    * From the spec:
    * The timeout value MUST be min(remaining connectTimeoutMS, remaining timeoutMS)
    * as described in the Server Selection section of the CSOT spec. If CSOT is
    * not applied, then the driver MUST use 1 minute as the timeout.
    *
    * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#oidc-callback
    */
   params = mongoc_oidc_callback_params_new ();
   mongoc_oidc_callback_params_set_user_data (params, mongoc_oidc_callback_get_user_data (oidc_callback));
   mongoc_oidc_callback_params_set_timeout (params, 60ll * 1000ll * 1000ll);

   /* Call the user provided callback function with params. */
   bson_mutex_lock (&_oidc_callback_mutex);
   mongoc_oidc_credential_t *creds = mongoc_oidc_callback_get_fn (oidc_callback) (params);
   bson_mutex_unlock (&_oidc_callback_mutex);

   if (creds) {
      /* Store the resulting access token in the client. */
      mongoc_oidc_credential_destroy (client->topology->oidc_credential);
      client->topology->oidc_credential = creds;
      ok = true;
   } else {
      MONGOC_ERROR ("error from within user provided OIDC callback");
   }

unlock_oidc_mutex:
   bson_mutex_unlock (&client->topology->oidc_mtx);
done:
   mongoc_oidc_callback_params_destroy (params);
   return ok;
}

static void
bson_zero_destroy (bson_t *bson)
{
   if (bson) {
      uint32_t length;
      uint8_t *data = bson_destroy_with_steal (bson, true, &length);
      BSON_ASSERT (data);
      bson_zero_free (data, length);
   }
}

/*
 * Authenticate with the server using the OIDC SASL One Step Conversation.
 * Before calling this function, you must first populate the client with an oidc_credential
 * using the _oidc_set_client_token() function.
 *
 * Copies the specific credential we've acquired, as a jwt document, to
 * the provided bson_t buffer. The intent is that this buffer can be preserved
 * temporarily for invalidating the specific token on error. (Invalidation requires
 * that we name a specific token, in order to support concurrent cache use by other
 * threads.)
 *
 * Spec:
 * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#one-step
 */
static bool
_oidc_sasl_one_step_conversation (mongoc_cluster_t *cluster,
                                  mongoc_stream_t *stream,
                                  mongoc_server_description_t *sd,
                                  bson_t *jwt_doc,
                                  bson_error_t *error)
{
   BSON_ASSERT_PARAM (cluster);
   BSON_ASSERT_PARAM (stream);
   BSON_ASSERT_PARAM (sd);
   BSON_ASSERT_PARAM (jwt_doc);
   BSON_OPTIONAL_PARAM (error);
   BSON_OPTIONAL_PARAM (failed_token);

   BSON_ASSERT (cluster->client);
   mongoc_topology_t *topology = cluster->client->topology;
   BSON_ASSERT (topology);

   bool ok = true;
   bson_t client_command = BSON_INITIALIZER;
   bson_t server_reply = BSON_INITIALIZER;
   bson_iter_t iter;
   int conv_id = 0;

   bson_mutex_lock (&topology->oidc_mtx);
   bson_append_utf8 (jwt_doc, "jwt", -1, mongoc_oidc_credential_get_access_token (topology->oidc_credential), -1);
   bson_mutex_unlock (&topology->oidc_mtx);

   BCON_APPEND (&client_command,
                "saslStart",
                BCON_INT32 (1),
                "mechanism",
                "MONGODB-OIDC",
                "payload",
                BCON_BIN (BSON_SUBTYPE_BINARY, bson_get_data (jwt_doc), jwt_doc->len));

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
   bson_zero_destroy (&client_command);
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
   bson_t jwt_doc = BSON_INITIALIZER;

   BSON_ASSERT (sd);
   BSON_ASSERT (error);
   BSON_ASSERT (stream);
   BSON_ASSERT (cluster);

   bson_once (&_init_oidc_callback_mutex_once_control, _mongoc_init_oidc_callback_mutex);

   /*
    * Fetch an OIDC access token using the user's callback function.
    * Store the access token in the client. (In a shared cache)
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
    * Uses the latest cached token, which is almost certainly the one set above.
    *
    * Spec:
    * https://github.com/mongodb/specifications/blob/master/source/auth/auth.md#conversation-6
    */
   bson_reinit (&jwt_doc);
   ok = _oidc_sasl_one_step_conversation (cluster, stream, sd, &jwt_doc, error);
   if (!ok && is_cache && first_time) {
      first_time = false;

      /* Invalidate the token cache before retrying, if it still contains the same token we captured and tried above. */
      bson_iter_t jwt_iter;
      BSON_ASSERT (bson_iter_init_find (&jwt_iter, &jwt_doc, "jwt"));
      BSON_ASSERT (BSON_ITER_HOLDS_UTF8 (&jwt_iter));
      mongoc_client_oidc_credential_invalidate (cluster->client, bson_iter_utf8 (&jwt_iter, NULL));

      _mongoc_usleep (100);
      goto again;
   }
   if (!ok) {
      goto fail;
   }

fail:
   bson_zero_destroy (&jwt_doc);
   return ok;
}

bool
_mongoc_cluster_oidc_reauthenticate (mongoc_cluster_t *cluster,
                                     mongoc_stream_t *stream,
                                     mongoc_server_description_t *sd,
                                     bson_error_t *error)
{
   /* TODO: This invalidates the current cached token, which may not be the token this client authenticated with. The
    * result is a race condition that can cause an unnecessary invalidation when multiple clients on a shared pool have
    * some overlap in their handling of a pool-wide reauthentication. */
   bson_mutex_lock (&cluster->client->topology->oidc_mtx);
   mongoc_oidc_credential_t *oidc_credential = cluster->client->topology->oidc_credential;
   char *cached_token =
      oidc_credential ? bson_strdup (mongoc_oidc_credential_get_access_token (oidc_credential)) : NULL;
   bson_mutex_unlock (&cluster->client->topology->oidc_mtx);

   mongoc_client_oidc_credential_invalidate (cluster->client, cached_token);

   if (cached_token) {
      bson_zero_free (cached_token, strlen (cached_token));
   }

   return _mongoc_cluster_auth_node_oidc (cluster, stream, sd, error);
}

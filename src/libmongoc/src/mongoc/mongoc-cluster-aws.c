/*
 * Copyright 2020-present MongoDB, Inc.
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

/* All interaction with kms_message should be limited to this file. */

#include "mongoc-cluster-aws-private.h"

#include "mongoc-client-private.h"
#include "mongoc-rand-private.h"
#include "mongoc-uri-private.h"

#define AUTH_ERROR_AND_FAIL(...)                     \
   bson_set_error (error,                            \
                   MONGOC_ERROR_CLIENT,              \
                   MONGOC_ERROR_CLIENT_AUTHENTICATE, \
                   __VA_ARGS__);                     \
   goto fail;


#ifdef MONGOC_ENABLE_MONGODB_AWS_AUTH
#include "kms_message/kms_message.h"

/*
 * Run a single command on a stream.
 *
 * On success, returns true.
 * On failure, returns false and sets error.
 * reply is always initialized.
 */
static bool
_run_command (mongoc_cluster_t *cluster,
              mongoc_stream_t *stream,
              mongoc_server_description_t *sd,
              bson_t *command,
              bson_t *reply,
              bson_error_t *error)
{
   mongoc_cmd_parts_t parts;
   mongoc_server_stream_t *server_stream;
   bool ret;

   mongoc_cmd_parts_init (&parts,
                          cluster->client,
                          "$external",
                          MONGOC_QUERY_NONE /* unused for OP_MSG */,
                          command);
   /* Drivers must not append session ids to auth commands per sessions spec. */
   parts.prohibit_lsid = true;
   server_stream = _mongoc_cluster_create_server_stream (
      cluster->client->topology, sd->id, stream, error);
   if (!server_stream) {
      /* error was set by mongoc_topology_description_server_by_id */
      bson_init (reply);
      return false;
   }
   ret = mongoc_cluster_run_command_parts (
      cluster, server_stream, &parts, reply, error);
   mongoc_server_stream_cleanup (server_stream);
   return ret;
}

/*
 * Utility function to parse out a server reply's payload.
 *
 * Given a server reply like { ok: 1, payload: <BSON data>, ... } parse out the
 * payload into a bson_t.
 * On success, returns true.
 * On failure, returns false and sets error.
 * payload is always initialized.
 */
static bool
_sasl_reply_parse_payload_as_bson (const bson_t *reply,
                                   bson_t *payload,
                                   bson_error_t *error)
{
   bson_iter_t iter;
   bson_subtype_t payload_subtype;
   const uint8_t *payload_data;
   uint32_t payload_len;
   bool ret = false;

   bson_init (payload);

   if (!bson_iter_init_find (&iter, reply, "payload") ||
       !BSON_ITER_HOLDS_BINARY (&iter)) {
      AUTH_ERROR_AND_FAIL ("server reply did not contain binary payload");
   }

   bson_iter_binary (&iter, &payload_subtype, &payload_len, &payload_data);

   if (payload_subtype != BSON_SUBTYPE_BINARY) {
      AUTH_ERROR_AND_FAIL ("server reply contained unexpected binary subtype");
   }

   bson_destroy (payload);
   if (!bson_init_static (payload, payload_data, payload_len)) {
      AUTH_ERROR_AND_FAIL ("server payload is invalid BSON");
   }

   ret = true;
fail:
   return ret;
}

typedef struct {
   char *access_key_id;
   char *secret_access_key;
   /* TODO: add session token */
} aws_credentials_t;

/*
 * Attempt to obtain AWS credentials.
 *
 * Credentials may be passed in multiple ways. The precedence is as follows:
 * 1. Username/password in the URI (and authMechanismProperty for session token)
 * 2. From environment variables.
 * 3. From querying the ECS local HTTP server.
 * 4. From querying the EC2 local HTTP server.
 *
 * On success, returns true.
 * On failure, returns false and sets error.
 */
static bool
_aws_credentials_obtain (mongoc_uri_t *uri,
                         aws_credentials_t *creds,
                         bson_error_t *error)
{
   bool ret = false;
   const char *username;
   const char *password;

   username = mongoc_uri_get_username (uri);
   password = mongoc_uri_get_password (uri);

   if (username) {
      if (!password || 0 == strlen (password)) {
         AUTH_ERROR_AND_FAIL ("Username provided but no password provided.")
      }
      creds->access_key_id = bson_strdup (username);
      creds->secret_access_key = bson_strdup (password);
   } else {
      AUTH_ERROR_AND_FAIL ("TODO - not implemented yet.")
   }

   ret = true;
fail:
   return ret;
}

static void
_aws_credentials_cleanup (aws_credentials_t *creds)
{
   bson_free (creds->access_key_id);
   bson_free (creds->secret_access_key);
}

/* --------------------------------------------------------------------------
 * Step 1
 * --------------------------------------------------------------------------
 * Client sends BSON payload:
 * {
 *   "r": <32 byte client nonce>,
 *   "p": 110
 * }
 * Server responds with BSON payload:
 * {
 *   "s": <32 byte client nonce + 32 byte server nonce>,
 *   "h": <domain name of STS service>
 * }
 *
 * Payloads are wrapped in SASL commands. The command a client sends is like:
 * { "saslStart": 1, "mechanism": "MONGODB-AWS", "payload": <BSON payload> }
 * And similar for server responses:
 * { "ok": 1, "conversationId": 1, "done": false, "payload": <BSON payload> }
 *
 * On success, returns true.
 * On failure, returns false and sets error.
 * --------------------------------------------------------------------------
 */
static bool
_client_first (mongoc_cluster_t *cluster,
               mongoc_stream_t *stream,
               mongoc_server_description_t *sd,
               uint8_t *server_nonce,
               char **sts_fqdn,
               int *conv_id,
               bson_error_t *error)
{
   bool ret = false;
   uint8_t client_nonce[32];
   bson_t client_payload = BSON_INITIALIZER;
   bson_t client_command = BSON_INITIALIZER;
   bson_t server_payload = BSON_INITIALIZER;
   bson_t server_reply = BSON_INITIALIZER;
   bson_iter_t iter;
   bson_subtype_t reply_nonce_subtype;
   const uint8_t *reply_nonce_data;
   uint32_t reply_nonce_len;

   /* Reset out params. */
   memset (server_nonce, 0, 32);
   *sts_fqdn = NULL;
   *conv_id = 0;

#ifdef MONGOC_ENABLE_CRYPTO
   /* Generate secure random nonce. */
   if (!_mongoc_rand_bytes (client_nonce, 32)) {
      AUTH_ERROR_AND_FAIL ("Could not generate client nonce");
   }
#else
   AUTH_ERROR_AND_FAIL ("libmongoc requires a cryptography library (libcrypto, "
                        "Common Crypto, or cng) to support MONGODB-AWS")
#endif

   BCON_APPEND (&client_payload,
                "r",
                BCON_BIN (BSON_SUBTYPE_BINARY, client_nonce, 32),
                "p",
                BCON_INT32 (110));

   BCON_APPEND (&client_command,
                "saslStart",
                BCON_INT32 (1),
                "mechanism",
                "MONGODB-AWS",
                "payload",
                BCON_BIN (BSON_SUBTYPE_BINARY,
                          bson_get_data (&client_payload),
                          client_payload.len));

   bson_destroy (&server_reply);
   if (!_run_command (
          cluster, stream, sd, &client_command, &server_reply, error)) {
      goto fail;
   }

   *conv_id = _mongoc_cluster_get_conversation_id (&server_reply);
   if (!*conv_id) {
      AUTH_ERROR_AND_FAIL ("server reply did not contain conversationId");
   }

   if (!_sasl_reply_parse_payload_as_bson (
          &server_reply, &server_payload, error)) {
      goto fail;
   }

   if (!bson_iter_init_find (&iter, &server_payload, "h") ||
       !BSON_ITER_HOLDS_UTF8 (&iter)) {
      AUTH_ERROR_AND_FAIL ("server payload did not contain string STS FQDN");
   }
   *sts_fqdn = bson_strdup (bson_iter_utf8 (&iter, NULL));

   if (!bson_iter_init_find (&iter, &server_payload, "s") ||
       !BSON_ITER_HOLDS_BINARY (&iter)) {
      AUTH_ERROR_AND_FAIL ("server payload did not contain nonce");
   }

   bson_iter_binary (
      &iter, &reply_nonce_subtype, &reply_nonce_len, &reply_nonce_data);
   if (reply_nonce_len != 64) {
      AUTH_ERROR_AND_FAIL ("server reply nonce was not 64 bytes");
   }

   if (0 != memcmp (reply_nonce_data, client_nonce, 32)) {
      AUTH_ERROR_AND_FAIL (
         "server reply nonce prefix did not match client nonce");
   }

   memcpy (server_nonce, reply_nonce_data + 32, 32);

   ret = true;
fail:
   bson_destroy (&client_payload);
   bson_destroy (&client_command);
   bson_destroy (&server_reply);
   bson_destroy (&server_payload);
   return ret;
}

/* --------------------------------------------------------------------------
 * Step 2
 * --------------------------------------------------------------------------
 * Client sends BSON payload:
 * {
 *   "a": <signed headers>,
 *   "d": <current date in UTC>
 *   "t": <optional security token>
 * }
 *
 * Server responds with final result.
 *
 * On success, returns true.
 * On failure, returns false and sets error.
 * --------------------------------------------------------------------------
 */
static bool
_client_second (mongoc_cluster_t *cluster,
                mongoc_stream_t *stream,
                mongoc_server_description_t *sd,
                aws_credentials_t *creds,
                const uint8_t *server_nonce,
                const char *sts_fqdn,
                int conv_id,
                bson_error_t *error)
{
   bool ret = false;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);
   BSON_ASSERT (sd);
   BSON_ASSERT (creds);
   BSON_ASSERT (server_nonce);
   BSON_ASSERT (sts_fqdn);
   BSON_ASSERT (conv_id);
   AUTH_ERROR_AND_FAIL ("TODO - step 2 not implemented yet");
fail:
   return ret;
}

bool
_mongoc_cluster_auth_node_aws (mongoc_cluster_t *cluster,
                               mongoc_stream_t *stream,
                               mongoc_server_description_t *sd,
                               bson_error_t *error)
{
   bool ret = false;
   uint8_t server_nonce[32];
   char *sts_fqdn = NULL;
   int conv_id = 0;
   aws_credentials_t creds = {0};

   if (!_aws_credentials_obtain (cluster->client->uri, &creds, error)) {
      goto fail;
   }

   if (!_client_first (
          cluster, stream, sd, server_nonce, &sts_fqdn, &conv_id, error)) {
      goto fail;
   }

   if (!_client_second (cluster,
                        stream,
                        sd,
                        &creds,
                        server_nonce,
                        sts_fqdn,
                        conv_id,
                        error)) {
      goto fail;
   }

   ret = true;
fail:
   _aws_credentials_cleanup (&creds);
   bson_free (sts_fqdn);
   return ret;
}

#else

bool
_mongoc_cluster_auth_node_aws (mongoc_cluster_t *cluster,
                               mongoc_stream_t *stream,
                               mongoc_server_description_t *sd,
                               bson_error_t *error)
{
   AUTH_ERROR_AND_FAIL ("AWS auth not supported, configure libmongoc with "
                        "ENABLE_MONGODB_AWS_AUTH=ON")
fail:
   return false;
}

#endif /* MONGOC_ENABLE_MONGODB_AWS_AUTH */
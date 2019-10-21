/*
 * Copyright 2019-present MongoDB, Inc.
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

#define MONGOC_LOG_DOMAIN "client-side-encryption"

#include "mongoc/mongoc-client-side-encryption-private.h"

#include "mongoc/mongoc-config.h"

/* Auto encryption opts. */
struct _mongoc_auto_encryption_opts_t {
   /* key_vault_client is not owned and must outlive auto encrypted client. */
   mongoc_client_t *key_vault_client;
   char *db;
   char *coll;
   bson_t *kms_providers;
   bson_t *schema_map;
   bool bypass_auto_encryption;
   bson_t *extra;
};

mongoc_auto_encryption_opts_t *
mongoc_auto_encryption_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_auto_encryption_opts_t));
}

void
mongoc_auto_encryption_opts_destroy (mongoc_auto_encryption_opts_t *opts)
{
   bson_destroy (opts->extra);
   bson_destroy (opts->kms_providers);
   bson_destroy (opts->schema_map);
   bson_free (opts->db);
   bson_free (opts->coll);
   bson_free (opts);
}

void
mongoc_auto_encryption_opts_set_key_vault_client (
   mongoc_auto_encryption_opts_t *opts, mongoc_client_t *client)
{
   /* Does not own. */
   opts->key_vault_client = client;
}

void
mongoc_auto_encryption_opts_set_key_vault_namespace (
   mongoc_auto_encryption_opts_t *opts, const char *db, const char *coll)
{
   bson_free (opts->db);
   opts->db = bson_strdup (db);
   bson_free (opts->coll);
   opts->coll = bson_strdup (coll);
}

void
mongoc_auto_encryption_opts_set_kms_providers (
   mongoc_auto_encryption_opts_t *opts, const bson_t *providers)
{
   bson_destroy (opts->kms_providers);
   opts->kms_providers = NULL;
   if (providers) {
      opts->kms_providers = bson_copy (providers);
   }
}

void
mongoc_auto_encryption_opts_set_schema_map (mongoc_auto_encryption_opts_t *opts,
                                            const bson_t *schema_map)
{
   bson_destroy (opts->schema_map);
   opts->schema_map = NULL;
   if (schema_map) {
      opts->schema_map = bson_copy (schema_map);
   }
}

void
mongoc_auto_encryption_opts_set_bypass_auto_encryption (
   mongoc_auto_encryption_opts_t *opts, bool bypass_auto_encryption)
{
   opts->bypass_auto_encryption = bypass_auto_encryption;
}

void
mongoc_auto_encryption_opts_set_extra (mongoc_auto_encryption_opts_t *opts,
                                       const bson_t *extra)
{
   bson_destroy (opts->extra);
   opts->extra = NULL;
   if (extra) {
      opts->extra = bson_copy (extra);
   }
}


#ifndef MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION

bool
_mongoc_cse_auto_encrypt (mongoc_client_t *client,
                          const mongoc_cmd_t *cmd,
                          mongoc_cmd_t *encrypted_cmd,
                          bson_t *encrypted,
                          bson_error_t *error)
{
   bson_init (encrypted);
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

bool
_mongoc_cse_auto_decrypt (mongoc_client_t *client,
                          const char *db_name,
                          const bson_t *reply,
                          bson_t *decrypted,
                          bson_error_t *error)
{
   bson_init (decrypted);
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

bool
_mongoc_cse_enable_auto_encryption (
   mongoc_client_t *client,
   mongoc_auto_encryption_opts_t *opts /* may be NULL */,
   bson_error_t *error)
{
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

#else

#include <mongocrypt/mongocrypt.h>

#include "mongoc/mongoc-client-private.h"
#include "mongoc/mongoc-stream-private.h"
#include "mongoc/mongoc-host-list-private.h"
#include "mongoc/mongoc-trace-private.h"
#include "mongoc/mongoc-util-private.h"

static void
_prefix_mongocryptd_error (bson_error_t *error)
{
   char buf[sizeof (error->message)];

   bson_snprintf (buf, sizeof (buf), "mongocryptd error: %s:", error->message);
   memcpy (error->message, buf, sizeof (buf));
}

static void
_prefix_key_vault_error (bson_error_t *error)
{
   char buf[sizeof (error->message)];

   bson_snprintf (buf, sizeof (buf), "key vault error: %s:", error->message);
   memcpy (error->message, buf, sizeof (buf));
}

static void
_status_to_error (mongocrypt_status_t *status, bson_error_t *error)
{
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT_SIDE_ENCRYPTION,
                   mongocrypt_status_code (status),
                   "%s",
                   mongocrypt_status_message (status, NULL));
}

/* Checks for an error on mongocrypt context.
 * If error_expected, then we expect mongocrypt_ctx_status to report a failure
 * status (due to a previous failed function call). If it did not, return a
 * generic error.
 * Returns true if ok, and does not modify @error.
 * Returns false if error, and sets @error.
 */
bool
_ctx_check_error (mongocrypt_ctx_t *ctx,
                  bson_error_t *error,
                  bool error_expected)
{
   mongocrypt_status_t *status;

   status = mongocrypt_status_new ();
   if (!mongocrypt_ctx_status (ctx, status)) {
      _status_to_error (status, error);
      mongocrypt_status_destroy (status);
      return false;
   } else if (error_expected) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "generic error from libmongocrypt operation");
      mongocrypt_status_destroy (status);
      return false;
   }
   mongocrypt_status_destroy (status);
   return true;
}

bool
_kms_ctx_check_error (mongocrypt_kms_ctx_t *kms_ctx,
                      bson_error_t *error,
                      bool error_expected)
{
   mongocrypt_status_t *status;

   status = mongocrypt_status_new ();
   if (!mongocrypt_kms_ctx_status (kms_ctx, status)) {
      _status_to_error (status, error);
      mongocrypt_status_destroy (status);
      return false;
   } else if (error_expected) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "generic error from libmongocrypt KMS operation");
      mongocrypt_status_destroy (status);
      return false;
   }
   mongocrypt_status_destroy (status);
   return true;
}

bool
_crypt_check_error (mongocrypt_t *crypt,
                    bson_error_t *error,
                    bool error_expected)
{
   mongocrypt_status_t *status;

   status = mongocrypt_status_new ();
   if (!mongocrypt_status (crypt, status)) {
      _status_to_error (status, error);
      mongocrypt_status_destroy (status);
      return false;
   } else if (error_expected) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "generic error from libmongocrypt handle");
      mongocrypt_status_destroy (status);
      return false;
   }
   mongocrypt_status_destroy (status);
   return true;
}

/* Convert a mongocrypt_binary_t to a static bson_t */
static bool
_bin_to_static_bson (mongocrypt_binary_t *bin, bson_t *out, bson_error_t *error)
{
   /* Copy bin into bson_t result. */
   if (!bson_init_static (
          out, mongocrypt_binary_data (bin), mongocrypt_binary_len (bin))) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "invalid returned bson");
      return false;
   }
   return true;
}

/* State handler MONGOCRYPT_CTX_NEED_MONGO_COLLINFO */
static bool
_state_need_mongo_collinfo (mongoc_client_t *client,
                            const char *db_name,
                            mongocrypt_ctx_t *ctx,
                            bson_error_t *error)
{
   mongoc_database_t *db = NULL;
   mongoc_cursor_t *cursor = NULL;
   bson_t filter_bson;
   const bson_t *collinfo_bson = NULL;
   bson_t opts = BSON_INITIALIZER;
   mongocrypt_binary_t *filter_bin = NULL;
   mongocrypt_binary_t *collinfo_bin = NULL;
   bool ret = false;

   /* 1. Run listCollections on the encrypted MongoClient with the filter
    * provided by mongocrypt_ctx_mongo_op */
   filter_bin = mongocrypt_binary_new ();
   if (!mongocrypt_ctx_mongo_op (ctx, filter_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   if (!_bin_to_static_bson (filter_bin, &filter_bson, error)) {
      goto fail;
   }

   bson_append_document (&opts, "filter", -1, &filter_bson);
   db = mongoc_client_get_database (client, db_name);
   cursor = mongoc_database_find_collections_with_opts (db, &opts);
   if (mongoc_cursor_error (cursor, error)) {
      goto fail;
   }

   /* 2. Return the first result (if any) with mongocrypt_ctx_mongo_feed or
    * proceed to the next step if nothing was returned. */
   if (mongoc_cursor_next (cursor, &collinfo_bson)) {
      collinfo_bin = mongocrypt_binary_new_from_data (
         (uint8_t *) bson_get_data (collinfo_bson), collinfo_bson->len);
      if (!mongocrypt_ctx_mongo_feed (ctx, collinfo_bin)) {
         _ctx_check_error (ctx, error, true);
         goto fail;
      }
   } else if (mongoc_cursor_error (cursor, error)) {
      goto fail;
   }

   /* 3. Call mongocrypt_ctx_mongo_done */
   if (!mongocrypt_ctx_mongo_done (ctx)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   ret = true;

fail:

   bson_destroy (&opts);
   mongocrypt_binary_destroy (filter_bin);
   mongocrypt_binary_destroy (collinfo_bin);
   mongoc_cursor_destroy (cursor);
   mongoc_database_destroy (db);
   return ret;
}

static bool
_state_need_mongo_markings (mongoc_client_t *client,
                            mongocrypt_ctx_t *ctx,
                            bson_error_t *error)
{
   bool ret = false;
   mongocrypt_binary_t *mongocryptd_cmd_bin = NULL;
   mongocrypt_binary_t *mongocryptd_reply_bin = NULL;
   bson_t mongocryptd_cmd_bson;
   bson_t reply = BSON_INITIALIZER;

   mongocryptd_cmd_bin = mongocrypt_binary_new ();

   if (!mongocrypt_ctx_mongo_op (ctx, mongocryptd_cmd_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   if (!_bin_to_static_bson (
          mongocryptd_cmd_bin, &mongocryptd_cmd_bson, error)) {
      goto fail;
   }

   /* 1. Use db.runCommand to run the command provided by
    * mongocrypt_ctx_mongo_op on the MongoClient connected to mongocryptd. */
   bson_destroy (&reply);
   if (!mongoc_client_command_simple (client->mongocryptd_client,
                                      "admin",
                                      &mongocryptd_cmd_bson,
                                      NULL /* read_prefs */,
                                      &reply,
                                      error)) {
      _prefix_mongocryptd_error (error);
      goto fail;
   }

   /* 2. Feed the reply back with mongocrypt_ctx_mongo_feed. */
   mongocryptd_reply_bin = mongocrypt_binary_new_from_data (
      (uint8_t *) bson_get_data (&reply), reply.len);
   if (!mongocrypt_ctx_mongo_feed (ctx, mongocryptd_reply_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   /* 3. Call mongocrypt_ctx_mongo_done. */
   if (!mongocrypt_ctx_mongo_done (ctx)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   ret = true;
fail:
   bson_destroy (&reply);
   mongocrypt_binary_destroy (mongocryptd_cmd_bin);
   mongocrypt_binary_destroy (mongocryptd_reply_bin);
   return ret;
}

static bool
_state_need_mongo_keys (mongoc_client_t *client,
                        mongocrypt_ctx_t *ctx,
                        bson_error_t *error)
{
   bool ret = false;
   mongocrypt_binary_t *filter_bin = NULL;
   bson_t filter_bson;
   bson_t opts = BSON_INITIALIZER;
   mongocrypt_binary_t *key_bin = NULL;
   const bson_t *key_bson;
   mongoc_cursor_t *cursor = NULL;
   mongoc_read_concern_t *rc = NULL;
   mongoc_collection_t *key_vault_coll = NULL;

   /* 1. Use MongoCollection.find on the MongoClient connected to the key vault
    * client (which may be the same as the encrypted client). Use the filter
    * provided by mongocrypt_ctx_mongo_op. */
   filter_bin = mongocrypt_binary_new ();
   if (!mongocrypt_ctx_mongo_op (ctx, filter_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   if (!_bin_to_static_bson (filter_bin, &filter_bson, error)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   rc = mongoc_read_concern_new ();
   mongoc_read_concern_set_level (rc, MONGOC_READ_CONCERN_LEVEL_MAJORITY);
   if (!mongoc_read_concern_append (rc, &opts)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "%s",
                      "could not set read concern");
      goto fail;
   }
   key_vault_coll = client->key_vault_coll;
   cursor = mongoc_collection_find_with_opts (
      key_vault_coll, &filter_bson, &opts, NULL /* read prefs */);
   /* 2. Feed all resulting documents back (if any) with repeated calls to
    * mongocrypt_ctx_mongo_feed. */
   while (mongoc_cursor_next (cursor, &key_bson)) {
      mongocrypt_binary_destroy (key_bin);
      key_bin = mongocrypt_binary_new_from_data (
         (uint8_t *) bson_get_data (key_bson), key_bson->len);
      if (!mongocrypt_ctx_mongo_feed (ctx, key_bin)) {
         _ctx_check_error (ctx, error, true);
         goto fail;
      }
   }
   if (mongoc_cursor_error (cursor, error)) {
      _prefix_key_vault_error (error);
      goto fail;
   }

   /* 3. Call mongocrypt_ctx_mongo_done. */
   if (!mongocrypt_ctx_mongo_done (ctx)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   ret = true;
fail:
   mongocrypt_binary_destroy (filter_bin);
   mongoc_cursor_destroy (cursor);
   mongoc_read_concern_destroy (rc);
   bson_destroy (&opts);
   mongocrypt_binary_destroy (key_bin);
   return ret;
}

static mongoc_stream_t *
_get_stream (const char *endpoint,
             int32_t connecttimeoutms,
             bson_error_t *error)
{
   mongoc_stream_t *base_stream = NULL;
   mongoc_stream_t *tls_stream = NULL;
   bool ret = false;
   mongoc_ssl_opt_t ssl_opts = {0};
   mongoc_host_list_t host;
   char *copied_endpoint = NULL;

   if (!strchr (endpoint, ':')) {
      copied_endpoint = bson_strdup_printf ("%s:443", endpoint);
   }

   if (!_mongoc_host_list_from_string_with_err (
          &host, copied_endpoint ? copied_endpoint : endpoint, error)) {
      goto fail;
   }

   base_stream = mongoc_client_connect_tcp (connecttimeoutms, &host, error);
   if (!base_stream) {
      goto fail;
   }

   /* Wrap in a tls_stream. */
   memcpy (&ssl_opts, mongoc_ssl_opt_get_default (), sizeof ssl_opts);
   tls_stream = mongoc_stream_tls_new_with_hostname (
      base_stream, endpoint, &ssl_opts, 1 /* client */);

   if (!mongoc_stream_tls_handshake_block (
          tls_stream, endpoint, connecttimeoutms, error)) {
      goto fail;
   }

   ret = true;
fail:
   bson_free (copied_endpoint);
   if (!ret) {
      if (tls_stream) {
         /* destroys base_stream too */
         mongoc_stream_destroy (tls_stream);
      } else if (base_stream) {
         mongoc_stream_destroy (base_stream);
      }
      return NULL;
   }
   return tls_stream;
}

static bool
_state_need_kms (mongoc_client_t *client,
                 mongocrypt_ctx_t *ctx,
                 bson_error_t *error)
{
   mongocrypt_kms_ctx_t *kms_ctx = NULL;
   mongoc_stream_t *tls_stream = NULL;
   bool ret = false;
   mongocrypt_binary_t *http_req = NULL;
   mongocrypt_binary_t *http_reply = NULL;
   const char *endpoint;

   kms_ctx = mongocrypt_ctx_next_kms_ctx (ctx);
   while (kms_ctx) {
      mongoc_iovec_t iov;

      mongocrypt_binary_destroy (http_req);
      http_req = mongocrypt_binary_new ();
      if (!mongocrypt_kms_ctx_message (kms_ctx, http_req)) {
         _kms_ctx_check_error (kms_ctx, error, true);
         goto fail;
      }

      if (!mongocrypt_kms_ctx_endpoint (kms_ctx, &endpoint)) {
         _kms_ctx_check_error (kms_ctx, error, true);
         goto fail;
      }

      tls_stream =
         _get_stream (endpoint, client->cluster.sockettimeoutms, error);
      if (!tls_stream) {
         goto fail;
      }

      iov.iov_base = (char *) mongocrypt_binary_data (http_req);
      iov.iov_len = mongocrypt_binary_len (http_req);

      if (!_mongoc_stream_writev_full (
             tls_stream, &iov, 1, client->cluster.sockettimeoutms, error)) {
         goto fail;
      }

      /* Read and feed reply. */
      while (mongocrypt_kms_ctx_bytes_needed (kms_ctx) > 0) {
         const int buf_size = 1024;
         uint8_t buf[buf_size];
         uint32_t bytes_needed = mongocrypt_kms_ctx_bytes_needed (kms_ctx);
         ssize_t read_ret;

         /* Cap the bytes requested at the buffer size. */
         if (bytes_needed > buf_size) {
            bytes_needed = buf_size;
         }

         read_ret = mongoc_stream_read (tls_stream,
                                        buf,
                                        bytes_needed,
                                        1 /* min_bytes. */,
                                        client->cluster.sockettimeoutms);
         if (read_ret == -1) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            "failed to read from KMS stream: %d",
                            errno);
            goto fail;
         }

         if (read_ret == 0) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            "unexpected EOF from KMS stream");
            goto fail;
         }

         mongocrypt_binary_destroy (http_reply);
         http_reply = mongocrypt_binary_new_from_data (buf, read_ret);
         if (!mongocrypt_kms_ctx_feed (kms_ctx, http_reply)) {
            _kms_ctx_check_error (kms_ctx, error, true);
            goto fail;
         }
      }
      kms_ctx = mongocrypt_ctx_next_kms_ctx (ctx);
   }
   /* When NULL is returned by mongocrypt_ctx_next_kms_ctx, this can either be
    * an error or end-of-list. */
   if (!_ctx_check_error (ctx, error, false)) {
      goto fail;
   }

   if (!mongocrypt_ctx_kms_done (ctx)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   ret = true;
fail:
   mongoc_stream_destroy (tls_stream);
   mongocrypt_binary_destroy (http_req);
   mongocrypt_binary_destroy (http_reply);
   return ret;
}

static bool
_state_ready (mongoc_client_t *client,
              mongocrypt_ctx_t *ctx,
              bson_t **result,
              bson_error_t *error)
{
   mongocrypt_binary_t *result_bin = NULL;
   bson_t tmp;
   bool ret = false;

   result_bin = mongocrypt_binary_new ();
   if (!mongocrypt_ctx_finalize (ctx, result_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   if (!_bin_to_static_bson (result_bin, &tmp, error)) {
      goto fail;
   }

   *result = bson_copy (&tmp);

   ret = true;
fail:
   mongocrypt_binary_destroy (result_bin);
   return ret;
}

/*--------------------------------------------------------------------------
 *
 * _mongoc_cse_run_state_machine --
 *    Run the mongocrypt_ctx state machine.
 *
 * Post-conditions:
 *    *result may be set to a new bson_t, or NULL otherwise. Caller should
 *    not assume return value of true means *result is set. If false returned,
 *    @error is set.
 *
 * --------------------------------------------------------------------------
 */
bool
_mongoc_cse_run_state_machine (mongoc_client_t *client,
                               const char *db_name,
                               mongocrypt_ctx_t *ctx,
                               bson_t **result,
                               bson_error_t *error)
{
   bool ret = false;
   mongocrypt_binary_t *bin = NULL;

   *result = NULL;
   while (true) {
      switch (mongocrypt_ctx_state (ctx)) {
      default:
      case MONGOCRYPT_CTX_ERROR:
         _ctx_check_error (ctx, error, true);
         goto fail;
      case MONGOCRYPT_CTX_NEED_MONGO_COLLINFO:
         if (!_state_need_mongo_collinfo (client, db_name, ctx, error)) {
            goto fail;
         }
         break;
      case MONGOCRYPT_CTX_NEED_MONGO_MARKINGS:
         if (!_state_need_mongo_markings (client, ctx, error)) {
            goto fail;
         }
         break;
      case MONGOCRYPT_CTX_NEED_MONGO_KEYS:
         if (!_state_need_mongo_keys (client, ctx, error)) {
            goto fail;
         }
         break;
      case MONGOCRYPT_CTX_NEED_KMS:
         if (!_state_need_kms (client, ctx, error)) {
            goto fail;
         }
         break;
      case MONGOCRYPT_CTX_READY:
         if (!_state_ready (client, ctx, result, error)) {
            goto fail;
         }
         break;
      case MONGOCRYPT_CTX_DONE:
         goto success;
         break;
      }
   }

success:
   ret = true;
fail:
   mongocrypt_binary_destroy (bin);
   return ret;
}


/*--------------------------------------------------------------------------
 *
 * _prep_for_auto_encryption --
 *    If @cmd contains a type=1 payload (document sequence), convert it into
 *    a type=0 payload (array payload). See OP_MSG spec for details.
 *    Place the command BSON that should be encrypted into @out.
 *
 * Post-conditions:
 *    @out is initialized and set to the full payload. If @cmd did not include
 *    a type=1 payload, @out is statically initialized. Caller must not modify
 *    @out after, but must call bson_destroy.
 *
 * --------------------------------------------------------------------------
 */
static void
_prep_for_auto_encryption (const mongoc_cmd_t *cmd, bson_t *out)
{
   /* If there is no type=1 payload, return the command unchanged. */
   if (!cmd->payload || !cmd->payload_size) {
      bson_init_static (out, bson_get_data (cmd->command), cmd->command->len);
      return;
   }

   /* Otherwise, append the type=1 payload as an array. */
   bson_copy_to (cmd->command, out);
   _mongoc_cmd_append_payload_as_array (cmd, out);
}

/*--------------------------------------------------------------------------
 *
 * _mongoc_cse_auto_encrypt --
 *
 *       Perform automatic encryption if enabled.
 *
 * Return:
 *       True on success, false on error.
 *
 * Pre-conditions:
 *       CSE is enabled on client.
 *
 * Post-conditions:
 *       If return false, @error is set. @encrypted is always initialized.
 *       @encrypted_cmd is set to the mongoc_cmd_t to send, which may refer
 *       to @encrypted.
 *       If automatic encryption was bypassed, @encrypted is set to an empty
 *       document, but @encrypted_cmd is a copy of @cmd. Caller must always
 *       bson_destroy @encrypted.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_cse_auto_encrypt (mongoc_client_t *client,
                          const mongoc_cmd_t *cmd,
                          mongoc_cmd_t *encrypted_cmd,
                          bson_t *encrypted,
                          bson_error_t *error)
{
   mongocrypt_ctx_t *ctx = NULL;
   mongocrypt_binary_t *cmd_bin = NULL;
   bool ret = false;
   bson_t cmd_bson = BSON_INITIALIZER;
   bson_t *result = NULL;
   bson_iter_t iter;

   ENTRY;

   bson_init (encrypted);

   if (client->bypass_auto_encryption) {
      memcpy (encrypted_cmd, cmd, sizeof (mongoc_cmd_t));
      return true;
   }

   if (!client->bypass_auto_encryption &&
       cmd->server_stream->sd->max_wire_version < WIRE_VERSION_CSE) {
      bson_set_error (
         error,
         MONGOC_ERROR_PROTOCOL,
         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
         "%s",
         "Auto-encryption requires a minimum MongoDB version of 4.2");
      goto fail;
   }

   /* Create the context for the operation. */
   ctx = mongocrypt_ctx_new (client->crypt);
   if (!ctx) {
      _crypt_check_error (client->crypt, error, true);
      goto fail;
   }

   /* Construct the command we're sending to libmongocrypt. If cmd includes a
    * type 1 payload, convert it to a type 0 payload. */
   bson_destroy (&cmd_bson);
   _prep_for_auto_encryption (cmd, &cmd_bson);
   cmd_bin = mongocrypt_binary_new_from_data (
      (uint8_t *) bson_get_data (&cmd_bson), cmd_bson.len);
   if (!mongocrypt_ctx_encrypt_init (ctx, cmd->db_name, -1, cmd_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   if (!_mongoc_cse_run_state_machine (
          client, cmd->db_name, ctx, &result, error)) {
      goto fail;
   }

   if (result) {
      bson_destroy (encrypted);
      bson_steal (encrypted, result);
      result = NULL;
   }

   /* Re-append $db if encryption stripped it. */
   if (!bson_iter_init_find (&iter, encrypted, "$db")) {
      BSON_APPEND_UTF8 (encrypted, "$db", cmd->db_name);
   }

   /* Create the modified cmd_t. */
   memcpy (encrypted_cmd, cmd, sizeof (mongoc_cmd_t));
   /* Modify the mongoc_cmd_t and clear the payload, since
    * _mongoc_cse_auto_encrypt converted the payload into an embedded array. */
   encrypted_cmd->payload = NULL;
   encrypted_cmd->payload_size = 0;
   encrypted_cmd->command = encrypted;

   ret = true;

fail:
   bson_destroy (result);
   bson_destroy (&cmd_bson);
   mongocrypt_binary_destroy (cmd_bin);
   mongocrypt_ctx_destroy (ctx);
   RETURN (ret);
}

/*--------------------------------------------------------------------------
 *
 * _mongoc_cse_auto_decrypt --
 *
 *       Perform automatic decryption.
 *
 * Return:
 *       True on success, false on error.
 *
 * Pre-conditions:
 *       FLE is enabled on client.
 *
 * Post-conditions:
 *       If return false, @error is set. @decrypted is always initialized.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_cse_auto_decrypt (mongoc_client_t *client,
                          const char *db_name,
                          const bson_t *reply,
                          bson_t *decrypted,
                          bson_error_t *error)
{
   mongocrypt_ctx_t *ctx = NULL;
   mongocrypt_binary_t *reply_bin = NULL;
   bool ret = false;
   bson_t *result = NULL;

   ENTRY;
   bson_init (decrypted);

   /* Create the context for the operation. */
   ctx = mongocrypt_ctx_new (client->crypt);
   if (!ctx) {
      _crypt_check_error (client->crypt, error, true);
      goto fail;
   }

   reply_bin = mongocrypt_binary_new_from_data (
      (uint8_t *) bson_get_data (reply), reply->len);
   if (!mongocrypt_ctx_decrypt_init (ctx, reply_bin)) {
      _ctx_check_error (ctx, error, true);
      goto fail;
   }

   if (!_mongoc_cse_run_state_machine (client, db_name, ctx, &result, error)) {
      goto fail;
   }

   if (result) {
      bson_destroy (decrypted);
      bson_steal (decrypted, result);
      result = NULL;
   }

   ret = true;

fail:
   bson_destroy (result);
   mongocrypt_binary_destroy (reply_bin);
   mongocrypt_ctx_destroy (ctx);
   RETURN (ret);
}

static void
_log_callback (mongocrypt_log_level_t mongocrypt_log_level,
               const char *message,
               uint32_t message_len,
               void *ctx)
{
   mongoc_log_level_t log_level = MONGOC_LOG_LEVEL_ERROR;

   switch (mongocrypt_log_level) {
   case MONGOCRYPT_LOG_LEVEL_FATAL:
      log_level = MONGOC_LOG_LEVEL_CRITICAL;
      break;
   case MONGOCRYPT_LOG_LEVEL_ERROR:
      log_level = MONGOC_LOG_LEVEL_ERROR;
      break;
   case MONGOCRYPT_LOG_LEVEL_WARNING:
      log_level = MONGOC_LOG_LEVEL_WARNING;
      break;
   case MONGOCRYPT_LOG_LEVEL_INFO:
      log_level = MONGOC_LOG_LEVEL_INFO;
      break;
   case MONGOCRYPT_LOG_LEVEL_TRACE:
      log_level = MONGOC_LOG_LEVEL_TRACE;
      break;
   }

   mongoc_log (log_level, MONGOC_LOG_DOMAIN, "%s", message, NULL);
}

static void
_uri_construction_error (bson_error_t *error)
{
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "Error constructing URI to mongocryptd");
}

bool
_mongoc_cse_enable_auto_encryption (mongoc_client_t *client,
                                    mongoc_auto_encryption_opts_t *opts,
                                    bson_error_t *error)
{
   bson_iter_t iter;
   bool ret = false;
   mongocrypt_binary_t *local_masterkey_bin = NULL;
   mongocrypt_binary_t *schema_map_bin = NULL;
   mongoc_uri_t *mongocryptd_uri = NULL;

   ENTRY;


   if (!client->topology->single_threaded) {
      bson_set_error (
         error,
         MONGOC_ERROR_CLIENT,
         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
         "Automatic encryption on pooled clients must be set on the pool");
      goto fail;
   }

   if (client->cse_enabled) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "Automatic encryption already set");
      goto fail;
   }

   if (!opts) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Auto encryption options required");
      goto fail;
   }

   /* Check for required options */
   if (!opts->db || !opts->coll) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Key vault namespace option required");
      goto fail;
   }

   if (!opts->kms_providers) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "KMS providers option required");
      goto fail;
   }

   client->cse_enabled = true;
   client->bypass_auto_encryption = opts->bypass_auto_encryption;

   if (!client->bypass_auto_encryption) {
      /* Spawn mongocryptd if needed, and create a client to it. */
      bool mongocryptd_bypass_spawn = false;
      const char *mongocryptd_spawn_path = NULL;
      bson_iter_t mongocryptd_spawn_args;
      bool has_spawn_args = false;

      if (opts->extra) {
         if (bson_iter_init_find (
                &iter, opts->extra, "mongocryptdBypassSpawn") &&
             bson_iter_as_bool (&iter)) {
            mongocryptd_bypass_spawn = true;
         }
         if (bson_iter_init_find (&iter, opts->extra, "mongocryptdSpawnPath") &&
             BSON_ITER_HOLDS_UTF8 (&iter)) {
            mongocryptd_spawn_path = bson_iter_utf8 (&iter, NULL);
         }
         if (bson_iter_init_find (&iter, opts->extra, "mongocryptdSpawnArgs") &&
             BSON_ITER_HOLDS_ARRAY (&iter)) {
            memcpy (&mongocryptd_spawn_args, &iter, sizeof (bson_iter_t));
            has_spawn_args = true;
         }

         if (bson_iter_init_find (&iter, opts->extra, "mongocryptdURI")) {
            if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
               bson_set_error (error,
                               MONGOC_ERROR_CLIENT,
                               MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                               "Expected string for option 'mongocryptdURI'");
               goto fail;
            }
            mongocryptd_uri =
               mongoc_uri_new_with_error (bson_iter_utf8 (&iter, NULL), error);
            if (!mongocryptd_uri) {
               goto fail;
            }
         }
      }

      if (!mongocryptd_bypass_spawn) {
         if (!_mongoc_fle_spawn_mongocryptd (
                mongocryptd_spawn_path,
                has_spawn_args ? &mongocryptd_spawn_args : NULL,
                error)) {
            goto fail;
         }
      }

      if (!mongocryptd_uri) {
         /* Always default to connecting to TCP, despite spec v1.0.0. Because
          * starting mongocryptd when one is running removes the domain socket
          * file per SERVER-41029. Connecting over TCP is more reliable.
          */
         mongocryptd_uri =
            mongoc_uri_new_with_error ("mongodb://localhost:27020", error);

         if (!mongocryptd_uri) {
            goto fail;
         }

         if (!mongoc_uri_set_option_as_int32 (
                mongocryptd_uri, MONGOC_URI_SERVERSELECTIONTIMEOUTMS, 5000)) {
            _uri_construction_error (error);
            goto fail;
         }
      }

      /* By default, single threaded clients set serverSelectionTryOnce to
       * true, which means server selection fails if a topology scan fails
       * the first time (i.e. it will not make repeat attempts until
       * serverSelectionTimeoutMS expires). Override this, since the first
       * attempt to connect to mongocryptd may fail when spawning, as it
       * takes some time for mongocryptd to listen on sockets. */
      if (!mongoc_uri_set_option_as_bool (
             mongocryptd_uri, MONGOC_URI_SERVERSELECTIONTRYONCE, false)) {
         _uri_construction_error (error);
         goto fail;
      }

      client->mongocryptd_client = mongoc_client_new_from_uri (mongocryptd_uri);

      if (!client->mongocryptd_client) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                         "Unable to create client to mongocryptd");
         goto fail;
      }
      /* Similarly, single threaded clients will by default wait for 5 second
       * cooldown period after failing to connect to a server before making
       * another attempt. Meaning if the first attempt to mongocryptd fails
       * to connect, then the user observes a 5 second delay. This is not
       * configurable in the URI, so override. */
      _mongoc_topology_bypass_cooldown (client->mongocryptd_client->topology);
   }

   /* Get the key vault collection. */
   if (opts->key_vault_client) {
      client->key_vault_coll = mongoc_client_get_collection (
         opts->key_vault_client, opts->db, opts->coll);
   } else {
      client->key_vault_coll =
         mongoc_client_get_collection (client, opts->db, opts->coll);
   }

   /* Create the handle to libmongocrypt. */
   client->crypt = mongocrypt_new ();

   mongocrypt_setopt_log_handler (
      client->crypt, _log_callback, NULL /* context */);

   /* Take options from the kms_providers map. */
   if (bson_iter_init_find (&iter, opts->kms_providers, "aws")) {
      bson_iter_t subiter;
      const char *aws_access_key_id = NULL;
      uint32_t aws_access_key_id_len = 0;
      const char *aws_secret_access_key = NULL;
      uint32_t aws_secret_access_key_len = 0;

      if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                         "Expected document for KMS provider 'aws'");
         goto fail;
      }

      BSON_ASSERT (bson_iter_recurse (&iter, &subiter));
      if (bson_iter_find (&subiter, "accessKeyId")) {
         aws_access_key_id = bson_iter_utf8 (&subiter, &aws_access_key_id_len);
      }

      BSON_ASSERT (bson_iter_recurse (&iter, &subiter));
      if (bson_iter_find (&subiter, "secretAccessKey")) {
         aws_secret_access_key =
            bson_iter_utf8 (&subiter, &aws_secret_access_key_len);
      }

      /* libmongocrypt returns error if options are NULL. */
      if (!mongocrypt_setopt_kms_provider_aws (client->crypt,
                                               aws_access_key_id,
                                               aws_access_key_id_len,
                                               aws_secret_access_key,
                                               aws_secret_access_key_len)) {
         _crypt_check_error (client->crypt, error, true);
         goto fail;
      }
   }

   if (bson_iter_init_find (&iter, opts->kms_providers, "local")) {
      bson_iter_t subiter;

      if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                         "Expected document for KMS provider 'local'");
         goto fail;
      }

      bson_iter_recurse (&iter, &subiter);
      if (bson_iter_find (&subiter, "key")) {
         uint32_t key_len;
         const uint8_t *key_data;

         bson_iter_binary (&subiter, NULL /* subtype */, &key_len, &key_data);
         local_masterkey_bin =
            mongocrypt_binary_new_from_data ((uint8_t *) key_data, key_len);
      }

      /* libmongocrypt returns error if options are NULL. */
      if (!mongocrypt_setopt_kms_provider_local (client->crypt,
                                                 local_masterkey_bin)) {
         _crypt_check_error (client->crypt, error, true);
         goto fail;
      }
   }

   if (opts->schema_map) {
      schema_map_bin = mongocrypt_binary_new_from_data (
         (uint8_t *) bson_get_data (opts->schema_map), opts->schema_map->len);
      if (!mongocrypt_setopt_schema_map (client->crypt, schema_map_bin)) {
         _crypt_check_error (client->crypt, error, true);
         goto fail;
      }
   }

   if (!mongocrypt_init (client->crypt)) {
      _crypt_check_error (client->crypt, error, true);
      goto fail;
   }

   ret = true;
fail:
   mongocrypt_binary_destroy (local_masterkey_bin);
   mongocrypt_binary_destroy (schema_map_bin);
   mongoc_uri_destroy (mongocryptd_uri);
   RETURN (ret);
}

#ifdef _WIN32
static bool
_do_spawn (const char *path, char **args, bson_error_t *error)
{
   bson_string_t *command;
   char **arg;
   PROCESS_INFORMATION process_information;
   STARTUPINFO startup_info;

   /* Construct the full command, quote path and arguments. */
   command = bson_string_new ("");
   bson_string_append (command, "\"");
   if (path) {
      bson_string_append (command, path);
   }
   bson_string_append (command, "mongocryptd.exe");
   bson_string_append (command, "\"");
   /* skip the "mongocryptd" first arg. */
   arg = args + 1;
   while (*arg) {
      bson_string_append (command, " \"");
      bson_string_append (command, *arg);
      bson_string_append (command, "\"");
      arg++;
   }

   ZeroMemory (&process_information, sizeof (process_information));
   ZeroMemory (&startup_info, sizeof (startup_info));

   startup_info.cb = sizeof (startup_info);

   if (!CreateProcessA (NULL,
                        command->str,
                        NULL,
                        NULL,
                        false /* inherit descriptors */,
                        DETACHED_PROCESS /* FLAGS */,
                        NULL /* environment */,
                        NULL /* current directory */,
                        &startup_info,
                        &process_information)) {
      long lastError = GetLastError ();
      LPSTR message = NULL;

      FormatMessageA (
         FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY |
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL,
         lastError,
         0,
         (LPSTR) &message,
         0,
         NULL);

      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "failed to spawn mongocryptd: %s",
                      message);
      LocalFree (message);
      bson_string_free (command, true);
      return false;
   }

   bson_string_free (command, true);
   return true;
}
#else

/*--------------------------------------------------------------------------
 *
 * _do_spawn --
 *
 *   Spawn process defined by arg[0] on POSIX systems.
 *
 *   Note, if mongocryptd fails to spawn (due to not being found on the path),
 *   an error is not reported and true is returned. Users will get an error
 *   later, upon first attempt to use mongocryptd.
 *
 *   These comments refer to three distinct processes: parent, child, and
 *   mongocryptd.
 *   - parent is initial calling process
 *   - child is the first forked child. It fork-execs mongocryptd then
 *     terminates. This makes mongocryptd an orphan, making it immediately
 *     adopted by the init process.
 *   - mongocryptd is the final background daemon (grandchild process).
 *
 * Return:
 *   False if an error definitely occurred. Returns true if no reportable
 *   error occurred (though an error may have occurred in starting
 *   mongocryptd, resulting in the process not running).
 *
 * Arguments:
 *    args - A NULL terminated list of arguments. The first argument MUST
 *    be the name of the process to execute, and the last argument MUST be
 *    NULL.
 *
 * Post-conditions:
 *    If return false, @error is set.
 *
 *--------------------------------------------------------------------------
 */
static bool
_do_spawn (const char *path, char **args, bson_error_t *error)
{
   pid_t pid;
   int fd;
   char *to_exec;

   /* Fork. The child will terminate immediately (after fork-exec'ing
    * mongocryptd). This orphans mongocryptd, and allows parent to wait on
    * child. */
   pid = fork ();
   if (pid < 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "failed to fork (errno=%d) '%s'",
                      errno,
                      strerror (errno));
      return false;
   } else if (pid > 0) {
      int child_status;

      /* Child will spawn mongocryptd and immediately terminate to turn
       * mongocryptd into an orphan. */
      if (waitpid (pid, &child_status, 0 /* options */) < 0) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                         "failed to wait for child (errno=%d) '%s'",
                         errno,
                         strerror (errno));
         return false;
      }
      /* parent is done at this point, return. */
      return true;
   }

   /* We're no longer in the parent process. Errors encountered result in an
    * exit.
    * Note, we're not logging here, because that would require the user's log
    * callback to be fork-safe.
    */

   /* Start a new session for the child, so it is not bound to the current
    * session (e.g. terminal session). */
   if (setsid () < 0) {
      exit (EXIT_FAILURE);
   }

   /* Fork again. Child terminates so mongocryptd gets orphaned and immedately
    * adopted by init. */
   signal (SIGHUP, SIG_IGN);
   pid = fork ();
   if (pid < 0) {
      exit (EXIT_FAILURE);
   } else if (pid > 0) {
      /* Child terminates immediately. */
      exit (EXIT_SUCCESS);
   }

   /* TODO: Depending on the outcome of MONGOCRYPT-115, possibly change the
    * process's working directory with chdir like: `chdir (default_pid_path)`.
    * Currently pid file ends up in application's working directory. */

   /* Set the user file creation mask to zero. */
   umask (0);

   /* Close and reopen stdin. */
   fd = open ("/dev/null", O_RDONLY);
   if (fd < 0) {
      exit (EXIT_FAILURE);
   }
   dup2 (fd, STDIN_FILENO);
   close (fd);

   /* Close and reopen stdout. */
   fd = open ("/dev/null", O_WRONLY);
   if (fd < 0) {
      exit (EXIT_FAILURE);
   }
   if (dup2 (fd, STDOUT_FILENO) < 0 || close (fd) < 0) {
      exit (EXIT_FAILURE);
   }

   /* Close and reopen stderr. */
   fd = open ("/dev/null", O_RDWR);
   if (fd < 0) {
      exit (EXIT_FAILURE);
   }
   if (dup2 (fd, STDERR_FILENO) < 0 || close (fd) < 0) {
      exit (EXIT_FAILURE);
   }
   fd = 0;

   if (path) {
      to_exec = bson_strdup_printf ("%s%s", path, args[0]);
   } else {
      to_exec = bson_strdup (args[0]);
   }
   if (execvp (to_exec, args) < 0) {
      /* Need to exit. */
      exit (EXIT_FAILURE);
   }

   /* Will never execute. */
   return false;
}
#endif

/*--------------------------------------------------------------------------
 *
 * _mongoc_fle_spawn_mongocryptd --
 *
 *   Attempt to spawn mongocryptd as a background process.
 *
 * Return:
 *   False if an error definitely occurred. Returns true if no reportable
 *   error occurred (though an error may have occurred in starting
 *   mongocryptd, resulting in the process not running).
 *
 * Arguments:
 *    mongocryptd_spawn_path May be NULL, otherwise the path to mongocryptd.
 *    mongocryptd_spawn_args May be NULL, otherwise a bson_iter_t to the
 *    value "mongocryptdSpawnArgs" in AutoEncryptionOpts.extraOptions
 *    (see spec).
 *
 * Post-conditions:
 *    If return false, @error is set.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_fle_spawn_mongocryptd (const char *mongocryptd_spawn_path,
                               const bson_iter_t *mongocryptd_spawn_args,
                               bson_error_t *error)
{
   char **args = NULL;
   bson_iter_t iter;
   bool passed_idle_shutdown_timeout_secs = false;
   int num_args = 2; /* for leading "mongocrypt" and trailing NULL */
   int i;

   /* iterate once to get length and validate all are strings */
   if (mongocryptd_spawn_args) {
      BSON_ASSERT (BSON_ITER_HOLDS_ARRAY (mongocryptd_spawn_args));
      bson_iter_recurse (mongocryptd_spawn_args, &iter);
      while (bson_iter_next (&iter)) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_CLIENT,
                            MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                            "invalid argument for mongocryptd, must be string");
            return false;
         }
         /* Check if the arg starts with --idleShutdownTimeoutSecs= or is equal
          * to --idleShutdownTimeoutSecs */
         if (0 == strncmp ("--idleShutdownTimeoutSecs=",
                           bson_iter_utf8 (&iter, NULL),
                           26) ||
             0 == strcmp ("--idleShutdownTimeoutSecs",
                          bson_iter_utf8 (&iter, NULL))) {
            passed_idle_shutdown_timeout_secs = true;
         }
         num_args++;
      }
   }

   if (!passed_idle_shutdown_timeout_secs) {
      /* add one more */
      num_args++;
   }

   args = (char **) bson_malloc (sizeof (char *) * num_args);
   i = 0;
   args[i++] = "mongocryptd";

   if (mongocryptd_spawn_args) {
      BSON_ASSERT (BSON_ITER_HOLDS_ARRAY (mongocryptd_spawn_args));
      bson_iter_recurse (mongocryptd_spawn_args, &iter);
      while (bson_iter_next (&iter)) {
         args[i++] = (char *) bson_iter_utf8 (&iter, NULL);
      }
   }

   if (!passed_idle_shutdown_timeout_secs) {
      args[i++] = "--idleShutdownTimeoutSecs=60";
   }

   BSON_ASSERT (i == num_args - 1);
   args[i++] = NULL;

   return _do_spawn (mongocryptd_spawn_path, args, error);
}

#endif /* MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION */
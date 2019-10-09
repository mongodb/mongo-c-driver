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

   /* Spawn mongocryptd (if applicable) and create a client to it. */
   if (opts->extra) {
      bson_iter_t subiter;

      if (bson_iter_init_find (&subiter, opts->extra, "mongocryptdURI")) {
         if (!BSON_ITER_HOLDS_UTF8 (&subiter)) {
            bson_set_error (error,
                            MONGOC_ERROR_CLIENT,
                            MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                            "Expected string for option 'mongocryptdURI'");
            goto fail;
         }
         mongocryptd_uri =
            mongoc_uri_new_with_error (bson_iter_utf8 (&subiter, NULL), error);
         if (!mongocryptd_uri) {
            goto fail;
         }
      }

      /* TODO: parse spawning options and spawn mongocryptd */
   }

   if (!mongocryptd_uri) {
      /* Always default to connecting to TCP, despite spec v1.0.0. Because
       * starting mongocryptd when one is running removes the domain socket file
       * per SERVER-41029. Connecting over TCP is more reliable. */
      mongocryptd_uri = mongoc_uri_new_with_error (
         "mongodb://localhost:27020/?serverSelectionTimeoutMS=1000", error);
      if (!mongocryptd_uri) {
         goto fail;
      }
   }

   client->mongocryptd_client = mongoc_client_new_from_uri (mongocryptd_uri);
   if (!client->mongocryptd_client) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "Unable to create client to mongocryptd");
      goto fail;
   }

   ret = true;
fail:
   mongocrypt_binary_destroy (local_masterkey_bin);
   mongocrypt_binary_destroy (schema_map_bin);
   mongoc_uri_destroy (mongocryptd_uri);
   RETURN (ret);
}


#endif /* MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION */
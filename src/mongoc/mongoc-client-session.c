/*
 * Copyright 2017 MongoDB, Inc.
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


#include "mongoc-client-session-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-client-private.h"
#include "mongoc-rand-private.h"


mongoc_session_opt_t *
mongoc_session_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_session_opt_t));
}


void
mongoc_session_opts_set_causally_consistent_reads (
   mongoc_session_opt_t *opts, bool causally_consistent_reads)
{
   ENTRY;

   BSON_ASSERT (opts);

   if (causally_consistent_reads) {
      opts->flags |= MONGOC_SESSION_CAUSALLY_CONSISTENT_READS;
   } else {
      opts->flags &= ~MONGOC_SESSION_CAUSALLY_CONSISTENT_READS;
   }

   EXIT;
}

bool
mongoc_session_opts_get_causally_consistent_reads (
   const mongoc_session_opt_t *opts)
{
   ENTRY;

   BSON_ASSERT (opts);

   RETURN (!!(opts->flags & MONGOC_SESSION_CAUSALLY_CONSISTENT_READS));
}


mongoc_session_opt_t *
mongoc_session_opts_clone (const mongoc_session_opt_t *opts)
{
   mongoc_session_opt_t *cloned_opts;

   ENTRY;

   BSON_ASSERT (opts);

   cloned_opts = bson_malloc (sizeof (mongoc_session_opt_t));
   memcpy (cloned_opts, opts, sizeof (mongoc_session_opt_t));

   RETURN (cloned_opts);
}


void
mongoc_session_opts_destroy (mongoc_session_opt_t *opts)
{
   ENTRY;

   BSON_ASSERT (opts);

   bson_free (opts);

   EXIT;
}


static bool
_mongoc_client_session_uuid(uint8_t *data /* OUT */,
                            bson_error_t *error)
{
#ifdef MONGOC_ENABLE_CRYPTO
   /* https://tools.ietf.org/html/rfc4122#page-14
    *   o  Set the two most significant bits (bits 6 and 7) of the
    *      clock_seq_hi_and_reserved to zero and one, respectively.
    *
    *   o  Set the four most significant bits (bits 12 through 15) of the
    *      time_hi_and_version field to the 4-bit version number from
    *      Section 4.1.3.
    *
    *   o  Set all the other bits to randomly (or pseudo-randomly) chosen
    *      values.
   */

   if (!_mongoc_rand_bytes (data, 16)) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "mongoc_client_start_session could not generate UUID");

      return false;
   }

   data[6] = (uint8_t) (0x40 | (data[6] & 0xf));
   data[8] = (uint8_t) (0x80 | (data[8] & 0x3f));

   return true;
#else
   /* no _mongoc_rand_bytes without a crypto library */
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_AUTHENTICATE,
                   "mongoc_client_start_session requires a cryptography library"
                   " like OpenSSL, Secure Channel, Common Crypto, etc.");

   return false;
#endif
}


mongoc_client_session_t *
_mongoc_client_session_new (mongoc_client_t *client,
                            const mongoc_session_opt_t *opts,
                            bson_error_t *error)
{
   mongoc_client_session_t *session;
   uint8_t uuid_data[16];

   ENTRY;

   BSON_ASSERT (client);

   if (!_mongoc_client_session_uuid (uuid_data, error)) {
      RETURN (NULL);
   }

   session = bson_malloc0 (sizeof (mongoc_client_session_t));
   session->client = client;

   bson_init (&session->lsid);
   bson_append_binary (
      &session->lsid, "id", 2, BSON_SUBTYPE_UUID, uuid_data, sizeof uuid_data);

   if (opts) {
      memcpy (&session->opts, opts, sizeof *opts);
   } else {
      session->opts.flags = MONGOC_SESSION_NO_OPTS;
   }

   RETURN (session);
}


mongoc_client_t *
mongoc_client_session_get_client (mongoc_client_session_t *session)
{
   BSON_ASSERT (session);

   return session->client;
}


const mongoc_session_opt_t *
mongoc_client_session_get_opts (const mongoc_client_session_t *session)
{
   BSON_ASSERT (session);

   return &session->opts;
}


const bson_t *
mongoc_client_session_get_session_id (const mongoc_client_session_t *session)
{
   BSON_ASSERT (session);

   return &session->lsid;
}


void
mongoc_client_session_destroy (mongoc_client_session_t *session)
{
   ENTRY;

   BSON_ASSERT (session);

   bson_destroy (&session->lsid);
   bson_free (session);

   EXIT;
}

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

#include "mongoc-config.h"

#ifdef MONGOC_ENABLE_SASL
#ifdef MONGOC_ENABLE_SASL_CYRUS
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include "mongoc-sasl-private.h"
#include "mongoc-cluster-sasl-private.h"
#endif
#include "mongoc-cluster-private.h"
#include "mongoc-log.h"
#include "mongoc-trace-private.h"
#include "mongoc-stream-private.h"
#include "mongoc-stream-socket.h"
#include "mongoc-error.h"

void
_mongoc_cluster_build_sasl_start (bson_t *cmd,
                                  const char *mechanism,
                                  const char *buf,
                                  uint32_t buflen)
{
   BSON_APPEND_INT32 (cmd, "saslStart", 1);
   BSON_APPEND_UTF8 (cmd, "mechanism", "GSSAPI");
   bson_append_utf8 (cmd, "payload", 7, buf, buflen);
   BSON_APPEND_INT32 (cmd, "autoAuthorize", 1);
}
void
_mongoc_cluster_build_sasl_continue (bson_t *cmd,
                                     int conv_id,
                                     const char *buf,
                                     uint32_t buflen)
{
   BSON_APPEND_INT32 (cmd, "saslContinue", 1);
   BSON_APPEND_INT32 (cmd, "conversationId", conv_id);
   bson_append_utf8 (cmd, "payload", 7, buf, buflen);
}
int
_mongoc_cluster_get_conversation_id (const bson_t *reply)
{
   bson_iter_t iter;

   if (bson_iter_init_find (&iter, reply, "conversationId") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      return bson_iter_int32 (&iter);
   }

   return 0;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_get_canonicalized_name --
 *
 *       Query the node to get the canonicalized name. This may happen if
 *       the node has been accessed via an alias.
 *
 *       The gssapi code will use this if canonicalizeHostname is true.
 *
 *       Some underlying layers of krb might do this for us, but they can
 *       be disabled in krb.conf.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_cluster_get_canonicalized_name (mongoc_cluster_t *cluster,    /* IN */
                                        mongoc_stream_t *node_stream, /* IN */
                                        char *name,                   /* OUT */
                                        size_t namelen,               /* IN */
                                        bson_error_t *error)          /* OUT */
{
   mongoc_stream_t *stream;
   mongoc_stream_t *tmp;
   mongoc_socket_t *sock = NULL;
   char *canonicalized;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node_stream);
   BSON_ASSERT (name);

   /*
    * Find the underlying socket used in the stream chain.
    */
   for (stream = node_stream; stream;) {
      if ((tmp = mongoc_stream_get_base_stream (stream))) {
         stream = tmp;
         continue;
      }
      break;
   }

   BSON_ASSERT (stream);

   if (stream->type == MONGOC_STREAM_SOCKET) {
      sock =
         mongoc_stream_socket_get_socket ((mongoc_stream_socket_t *) stream);
      if (sock) {
         canonicalized = mongoc_socket_getnameinfo (sock);
         if (canonicalized) {
            bson_snprintf (name, namelen, "%s", canonicalized);
            bson_free (canonicalized);
            RETURN (true);
         }
      }
   }

   RETURN (false);
}
/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_sasl --
 *
 *       Perform authentication for a cluster node using SASL. This is
 *       only supported for GSSAPI at the moment.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       error may be set.
 *
 *--------------------------------------------------------------------------
 */

#ifdef MONGOC_ENABLE_SASL_CYRUS
bool
_mongoc_cluster_auth_node_sasl (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                const char *hostname,
                                bson_error_t *error)
{
   uint32_t buflen = 0;
   mongoc_sasl_t sasl;
   bson_iter_t iter;
   bool ret = false;
   char real_name[BSON_HOST_NAME_MAX + 1];
   const char *mechanism;
   const char *tmpstr;
   uint8_t buf[4096] = {0};
   bson_t cmd;
   bson_t reply;
   int conv_id = 0;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);

   _mongoc_sasl_init (&sasl);

   if ((mechanism = mongoc_uri_get_auth_mechanism (cluster->uri))) {
      if (!_mongoc_sasl_set_mechanism (&sasl, mechanism, error)) {
         goto failure;
      }
   }

   _mongoc_sasl_set_pass (&sasl, mongoc_uri_get_password (cluster->uri));
   _mongoc_sasl_set_user (&sasl, mongoc_uri_get_username (cluster->uri));
   _mongoc_sasl_set_properties (&sasl, cluster->uri);

   /*
    * If the URI requested canonicalizeHostname, we need to resolve the real
    * hostname for the IP Address and pass that to the SASL layer. Some
    * underlying GSSAPI layers will do this for us, but can be disabled in
    * their config (krb.conf).
    *
    * This allows the consumer to specify canonicalizeHostname=true in the URI
    * and have us do that for them.
    *
    * See CDRIVER-323 for more information.
    */
   if (sasl.canonicalize_host_name &&
       _mongoc_cluster_get_canonicalized_name (
          cluster, stream, real_name, sizeof real_name, error)) {
      _mongoc_sasl_set_service_host (&sasl, real_name);
   } else {
      _mongoc_sasl_set_service_host (&sasl, hostname);
   }

   for (;;) {
      if (!_mongoc_sasl_step (
             &sasl, buf, buflen, buf, sizeof buf, &buflen, error)) {
         goto failure;
      }

      bson_init (&cmd);

      if (sasl.step == 1) {
         _mongoc_cluster_build_sasl_start (
            &cmd, mechanism ? mechanism : "GSSAPI", (const char *) buf, buflen);
      } else {
         _mongoc_cluster_build_sasl_continue (
            &cmd, conv_id, (const char *) buf, buflen);
      }

      TRACE ("SASL: authenticating (step %d)", sasl.step);

      TRACE ("Sending: %s", bson_as_json (&cmd, NULL));
      if (!mongoc_cluster_run_command (cluster,
                                       stream,
                                       0,
                                       MONGOC_QUERY_SLAVE_OK,
                                       "$external",
                                       &cmd,
                                       &reply,
                                       error)) {
         TRACE ("Replied with: %s", bson_as_json (&reply, NULL));
         bson_destroy (&cmd);
         bson_destroy (&reply);
         goto failure;
      }
      TRACE ("Replied with: %s", bson_as_json (&reply, NULL));

      bson_destroy (&cmd);

      if (bson_iter_init_find (&iter, &reply, "done") &&
          bson_iter_as_bool (&iter)) {
         bson_destroy (&reply);
         break;
      }

      conv_id = _mongoc_cluster_get_conversation_id (&reply);

      if (!bson_iter_init_find (&iter, &reply, "payload") ||
          !BSON_ITER_HOLDS_UTF8 (&iter)) {
         MONGOC_DEBUG ("SASL: authentication failed");
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "Received invalid SASL reply from MongoDB server.");
         goto failure;
      }

      tmpstr = bson_iter_utf8 (&iter, &buflen);
      TRACE ("Got string: %s, (len=%" PRIu32 ")\n", tmpstr, buflen);

      if (buflen > sizeof buf) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "SASL reply from MongoDB is too large.");

         bson_destroy (&reply);
         goto failure;
      }

      memcpy (buf, tmpstr, buflen);

      bson_destroy (&reply);
   }

   TRACE ("%s", "SASL: authenticated");

   ret = true;

failure:
   _mongoc_sasl_destroy (&sasl);

   return ret;
}
#endif
#endif

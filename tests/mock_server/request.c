/*
 * Copyright 2015 MongoDB, Inc.
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


#include <mongoc-rpc-private.h>
#include "mongoc.h"

#include "mock-server.h"
#include "../test-conveniences.h"


bool is_command (const char *ns);

void request_from_query (request_t *request, const mongoc_rpc_t *rpc);

void request_from_killcursors (request_t *request, const mongoc_rpc_t *rpc);


request_t *
request_new (const mongoc_rpc_t *request_rpc,
             mock_server_t *server,
             mongoc_stream_t *client,
             uint16_t client_port)
{
   request_t *request = bson_malloc0 (sizeof *request);

   memcpy ((void *) &request->request_rpc,
           (void *) request_rpc,
           sizeof *request_rpc);

   request->opcode = (mongoc_opcode_t) request_rpc->header.opcode;
   request->server = server;
   request->client = client;
   request->client_port = client_port;
   _mongoc_array_init (&request->docs, sizeof (bson_t));

   switch (request->opcode) {
   case MONGOC_OPCODE_QUERY:
      request_from_query (request, request_rpc);
      break;

   case MONGOC_OPCODE_KILL_CURSORS:
      request_from_killcursors (request, request_rpc);
      break;

   case MONGOC_OPCODE_REPLY:
   case MONGOC_OPCODE_MSG:
   case MONGOC_OPCODE_UPDATE:
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_DELETE:
      fprintf (stderr, "Unimplemented opcode %d\n", request->opcode);
      abort ();
   }

   return request;
}


/* TODO: take file, line, function params from caller, wrap in macro */
bool
request_matches_query (const request_t *request,
                       const char *ns,
                       mongoc_query_flags_t flags,
                       uint32_t skip,
                       uint32_t n_return,
                       const char *query_json,
                       const char *fields_json,
                       bool is_command)
{
   const mongoc_rpc_t *rpc = &request->request_rpc;
   bson_t *doc;
   bool n_return_equal;

   assert (request->docs.len <= 2);

   /* TODO: make a good request repr, skip logging and say:
    *   request_t *expected = request_new_from_pattern (...);
    *   if (!request_matches (request, expected)) {
    *       MONGOC_ERROR ("expected %s, got %s",
    *                     request_repr (expected), request_repr (request));
    *       return false;
    *   }
    */
   if (request->is_command && !is_command) {
      MONGOC_ERROR ("expected query, got command");
      return false;
   }

   if (!request->is_command && is_command) {
      MONGOC_ERROR ("expected command, got query");
      return false;
   }

   if (rpc->header.opcode != MONGOC_OPCODE_QUERY) {
      MONGOC_ERROR ("request's opcode does not match QUERY");
      return false;
   }

   if (strcmp (rpc->query.collection, ns)) {
      MONGOC_ERROR ("request's namespace is '%s', expected '%s'",
                    request->request_rpc.query.collection, ns);
      return false;
   }

   if (rpc->query.flags != flags) {
      MONGOC_ERROR ("request's query flags don't match");
      return false;
   }

   if (rpc->query.skip != skip) {
      MONGOC_ERROR ("requests's skip = %d, expected %d",
                    rpc->query.skip, skip);
      return false;
   }

   n_return_equal = (rpc->query.n_return == n_return);

   if (! n_return_equal && abs (rpc->query.n_return) == 1) {
      /* quirk: commands from mongoc_client_command_simple have n_return 1,
       * from mongoc_topology_scanner_t have n_return -1
       */
      n_return_equal = abs (rpc->query.n_return) == abs (n_return);
   }

   if (! n_return_equal) {
      MONGOC_ERROR ("requests's n_return = %d, expected %d",
                    rpc->query.n_return, n_return);
      return false;
   }

   if (request->docs.len) {
      doc = _mongoc_array_index (&request->docs, bson_t *, 0);
   } else {
      doc = NULL;
   }

   if (!match_json (doc, query_json, is_command, __FILE__, __LINE__,
                    __FUNCTION__)) {
      /* match_json has logged the err */
      return false;
   }

   if (request->docs.len > 1) {
      doc = _mongoc_array_index (&request->docs, bson_t *, 1);
   } else {
      doc = NULL;
   }

   if (!match_json (doc, fields_json, false,
                    __FILE__, __LINE__, __FUNCTION__)) {
      /* match_json has logged the err */
      return false;
   }

   return true;
}


/* TODO: take file, line, function params from caller, wrap in macro */
bool
request_matches_kill_cursors (const request_t *request,
                              int64_t cursor_id)
{
   const mongoc_rpc_t *rpc = &request->request_rpc;

   if (rpc->header.opcode != MONGOC_OPCODE_KILL_CURSORS) {
      MONGOC_ERROR ("request's opcode does not match KILL_CURSORS");
      return false;
   }

   if (rpc->kill_cursors.n_cursors != 1) {
      MONGOC_ERROR ("request's n_cursors is %d, expected 1",
                    rpc->kill_cursors.n_cursors);
      return false;
   }

   if (rpc->kill_cursors.cursors[0] != cursor_id) {
      MONGOC_ERROR ("request's cursor_id %" PRId64 ", expected %" PRId64,
                    rpc->kill_cursors.cursors[0], cursor_id);
      return false;
   }

   return true;
}


/*--------------------------------------------------------------------------
 *
 * request_get_server_port --
 *
 *       Get the port of the server this request was sent to.
 *
 * Returns:
 *       A port number.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

uint16_t
request_get_server_port (request_t *request)
{
   return mock_server_get_port (request->server);
}


/*--------------------------------------------------------------------------
 *
 * request_destroy --
 *
 *       Free a request_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
request_destroy (request_t *request)
{
   size_t i;
   bson_t *doc;

   for (i = 0; i < request->docs.len; i++) {
      doc = _mongoc_array_index (&request->docs, bson_t *, i);
      bson_destroy (doc);
   }

   _mongoc_array_destroy (&request->docs);
   bson_free (request->command_name);
   bson_free (request->as_str);
   bson_free (request);
}


bool
is_command (const char *ns)
{
   size_t len = strlen (ns);
   const char *cmd = ".$cmd";
   size_t cmd_len = strlen (cmd);

   return len > cmd_len && !strncmp (ns + len - cmd_len, cmd, cmd_len);
}


char *
flags_str (uint32_t flags)
{
   mongoc_query_flags_t flag = (mongoc_query_flags_t) 1;
   bson_string_t *str = bson_string_new ("");
   bool begun = false;

   if (flags == MONGOC_QUERY_NONE) {
      bson_string_append (str, "0");
   } else {
      while (flag <= MONGOC_QUERY_PARTIAL) {
         flag <<= 1;

         if (flags & flag) {
            if (begun) {
               bson_string_append (str, "|");
            }

            begun = true;

            switch (flag) {
            case MONGOC_QUERY_TAILABLE_CURSOR:
               bson_string_append (str, "TAILABLE");
               break;
            case MONGOC_QUERY_SLAVE_OK:
               bson_string_append (str, "SLAVE_OK");
               break;
            case MONGOC_QUERY_OPLOG_REPLAY:
               bson_string_append (str, "OPLOG_REPLAY");
               break;
            case MONGOC_QUERY_NO_CURSOR_TIMEOUT:
               bson_string_append (str, "NO_TIMEOUT");
               break;
            case MONGOC_QUERY_AWAIT_DATA:
               bson_string_append (str, "AWAIT_DATA");
               break;
            case MONGOC_QUERY_EXHAUST:
               bson_string_append (str, "EXHAUST");
               break;
            case MONGOC_QUERY_PARTIAL:
               bson_string_append (str, "PARTIAL");
               break;
            case MONGOC_QUERY_NONE:
               assert (false);
            }
         }
      }
   }

   return bson_string_free (str, false);  /* detach buffer */
}


void
request_from_query (request_t *request,
                    const mongoc_rpc_t *rpc)
{
   int32_t len;
   bson_t *query;
   bson_iter_t iter;
   bson_string_t *query_as_str = bson_string_new ("");
   char *str;

   /* TODO: multiple docs */
   memcpy (&len, rpc->query.query, 4);
   len = BSON_UINT32_FROM_LE (len);
   query = bson_new_from_data (rpc->query.query, (size_t) len);
   assert (query);
   _mongoc_array_append_val (&request->docs, query);

   if (is_command (request->request_rpc.query.collection)) {
      request->is_command = true;

      if (bson_iter_init (&iter, query) && bson_iter_next (&iter)) {
         request->command_name = bson_strdup (bson_iter_key (&iter));
      } else {
         fprintf (stderr, "WARNING: no command name for %s\n",
                  request->request_rpc.query.collection);
      }
   }

   str = bson_as_json (query, NULL);
   bson_string_append (query_as_str, str);
   bson_free (str);

   bson_string_append (query_as_str, " flags=");

   str = flags_str (rpc->query.flags);
   bson_string_append (query_as_str, str);
   bson_free (str);

   request->as_str = bson_string_free (query_as_str, false);
}


void
request_from_killcursors (request_t *request,
                          const mongoc_rpc_t *rpc)
{
   /* protocol allows multiple cursor ids but we only implement one */
   assert (rpc->kill_cursors.n_cursors == 1);
   request->as_str = bson_strdup_printf ("OP_KILLCURSORS %" PRId64,
                                         rpc->kill_cursors.cursors[0]);
}

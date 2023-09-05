/*
 * Copyright 2023-present MongoDB, Inc.
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

#include "mongoc/mongoc-config.h"

#include "mongoc/mcd-rpc.h"
#include "mongoc/mongoc-array-private.h"
#include "mongoc/mongoc-cluster-private.h"
#include "mongoc/mongoc-grpc-private.h"
#include "mongoc/mongoc-rpc-private.h"
#include "mongoc/utlist.h"

#include "mongoc/mongoc-log.h"
#include "mongoc/mongoc-error.h"

#include <bson/bson.h>

#include <grpc/byte_buffer_reader.h>
#include <grpc/grpc_security.h>
#include <grpc/grpc.h>
#include <grpc/support/alloc.h>


typedef struct _op_tag_t op_tag_t;

static grpc_channel *
create_channel (void);

static grpc_call *
create_call (grpc_channel *channel, grpc_completion_queue *cq);

static void
send_close_from_client (grpc_call *call, bool ignore_call_error);

static void
recv_status_on_client (grpc_call *call);

static void
op_tag_destroy (op_tag_t *tag);

static bool
start_send_initial_metadata (mongoc_grpc_t *grpc, bson_error_t *error);

static bool
start_recv_initial_metadata (mongoc_grpc_t *grpc, bson_error_t *error);

static bool
start_send_message (mongoc_grpc_t *grpc,
                    int32_t request_id,
                    uint32_t flags,
                    const bson_t *cmd,
                    const char *payload_identifier,
                    const uint8_t *payload,
                    int32_t payload_size,
                    int32_t compressor_id,
                    int32_t compression_level,
                    bson_error_t *error);

static bool
start_recv_message (mongoc_grpc_t *grpc, bson_error_t *error);

static bool
handle_event (mongoc_grpc_t *grpc, grpc_event event, bson_error_t *error);


// gRPC Protocol: Clients MUST use the following constants when serializing
// commands to OP_MSG.
static int max_message_size_bytes = 48000000;


// gRPC POC: hard-coded constants specific to gRPC POC or Atlas Proxy.
const char *const poc_atlas_target = "host9.local.10gen.cc:9901";
const char *const poc_atlas_authority = "host.local.10gen.cc";
const char *const poc_atlas_method =
   "/mongodb.CommandService/UnauthenticatedCommandStream";


struct _mongoc_grpc_t {
   grpc_channel *channel;
   grpc_completion_queue *cq;
   grpc_call *call;
   op_tag_t *tags;
   mcd_rpc_message *rpc;
   bson_t *reply;

   bool timed_out;
};


mongoc_grpc_t *
mongoc_grpc_new (const char *target)
{
   BSON_ASSERT_PARAM (target);

   if (strcmp (target, poc_atlas_target) != 0) {
      MONGOC_WARNING (
         "gRPC POC: target '%s' does not match expected target '%s'",
         target,
         poc_atlas_target);
   }

   mongoc_grpc_t *const ret = bson_malloc (sizeof (*ret));

   *ret = (mongoc_grpc_t){
      .channel = create_channel (),
      .cq = grpc_completion_queue_create_for_next (NULL),
      .call = NULL,
      .tags = NULL,
      .reply = NULL,
      .rpc = mcd_rpc_message_new (),
      .timed_out = false,
   };

   // Creation errors will be reported via the recv_status_on_client RPC as a
   // call error or a status code with error string.
   BSON_ASSERT (ret->channel);
   BSON_ASSERT (ret->cq);

   ret->call = create_call (ret->channel, ret->cq);
   BSON_ASSERT (ret->call); // Ditto w.r.t. creation errors.

   // Unconditionally submit a recv_status_on_client RPC for error handling.
   recv_status_on_client (ret->call);

   return ret;
}

void
mongoc_grpc_destroy (mongoc_grpc_t *grpc)
{
   if (!grpc) {
      return;
   }

   bson_destroy (grpc->reply);
   mcd_rpc_message_destroy (grpc->rpc);

   // Be nice and send a close RPC before cancelling the call.
   send_close_from_client (grpc->call, true);
   grpc_call_unref (grpc->call);

   {
      const gpr_timespec infinite_deadline =
         gpr_inf_future (GPR_CLOCK_REALTIME);

      grpc_completion_queue_shutdown (grpc->cq);

      // The completion queue must be drained before destruction to avoid
      // leaking unhandled events.
      while (true) {
         const grpc_event event =
            grpc_completion_queue_next (grpc->cq, infinite_deadline, NULL);

         // Should never timeout given an infinite deadline.
         BSON_ASSERT (event.type != GRPC_QUEUE_TIMEOUT);

         // All possible events have been drained.
         if (event.type == GRPC_QUEUE_SHUTDOWN) {
            break;
         }

         op_tag_destroy (event.tag);
      }

      grpc_completion_queue_destroy (grpc->cq);
   }

   grpc_channel_destroy (grpc->channel);

   bson_free (grpc);
}

grpc_connectivity_state
mongoc_grpc_check_connectivity_state (mongoc_grpc_t *grpc)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (grpc->channel);

   return grpc_channel_check_connectivity_state (grpc->channel, 0);
}

void
mongoc_grpc_call_cancel (mongoc_grpc_t *grpc)
{
   BSON_ASSERT_PARAM (grpc);

   const grpc_call_error call_error = grpc_call_cancel (grpc->call, NULL);

   if (call_error != GRPC_CALL_OK) {
      MONGOC_WARNING ("gRPC error during call cancel: %s",
                      grpc_call_error_to_string (call_error));
   }
}

bool
mongoc_grpc_start_initial_metadata (mongoc_grpc_t *grpc, bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (error || true);

   if (!start_send_initial_metadata (grpc, error)) {
      goto fail;
   }

   if (!start_recv_initial_metadata (grpc, error)) {
      goto fail;
   }

   return true;

fail:
   mongoc_grpc_call_cancel (grpc);
   return false;
}

bool
mongoc_grpc_start_message (mongoc_grpc_t *grpc,
                           int32_t request_id,
                           uint32_t flags,
                           const bson_t *cmd,
                           int32_t compressor_id,
                           int32_t compression_level,
                           bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT_PARAM (cmd);
   BSON_ASSERT (error || true);

   if (!start_send_message (grpc,
                            request_id,
                            flags,
                            cmd,
                            NULL,
                            NULL,
                            0u,
                            compressor_id,
                            compression_level,
                            error)) {
      goto fail;
   }

   if (!start_recv_message (grpc, error)) {
      goto fail;
   }

   return true;

fail:
   mongoc_grpc_call_cancel (grpc);
   return false;
}

bool
mongoc_grpc_start_message_with_payload (mongoc_grpc_t *grpc,
                                        int32_t request_id,
                                        uint32_t flags,
                                        const bson_t *cmd,
                                        const char *payload_identifier,
                                        const uint8_t *payload,
                                        int32_t payload_size,
                                        int32_t compressor_id,
                                        int32_t compression_level,
                                        bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT_PARAM (cmd);
   BSON_ASSERT (payload_identifier || true);
   BSON_ASSERT (payload || true);
   BSON_ASSERT (error || true);

   if (!start_send_message (grpc,
                            request_id,
                            flags,
                            cmd,
                            payload_identifier,
                            payload,
                            payload_size,
                            compressor_id,
                            compression_level,
                            error)) {
      goto fail;
   }

   if (!start_recv_message (grpc, error)) {
      goto fail;
   }

   return true;

fail:
   mongoc_grpc_call_cancel (grpc);
   return false;
}

bool
mongoc_grpc_handle_events (mongoc_grpc_t *grpc,
                           gpr_timespec deadline,
                           bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (error || true);

   grpc->timed_out = false;

   while (grpc->tags) {
      const grpc_event event =
         grpc_completion_queue_next (grpc->cq, deadline, NULL);

      switch (event.type) {
      case GRPC_QUEUE_SHUTDOWN:
         BSON_UNREACHABLE ("premature completion queue shutdown");
         goto fail;

      case GRPC_QUEUE_TIMEOUT: {
         grpc->timed_out = true;
         bson_set_error (error,
                         MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_SOCKET,
                         "event timeout");
         goto fail;
      }

      case GRPC_OP_COMPLETE: {
         // Note: `handle_event()` removes the event tag from `grpc->tags`.
         if (!handle_event (grpc, event, error)) {
            goto fail;
         }

         continue;
      }
      }

      BSON_UNREACHABLE ("invalid gRPC completion type");
   }

   return true;

fail:
   mongoc_grpc_call_cancel (grpc);
   return false;
}

bool
mongoc_grpc_event_timed_out (const mongoc_grpc_t *grpc)
{
   BSON_ASSERT_PARAM (grpc);

   return grpc->timed_out;
}

void
mongoc_grpc_steal_reply (mongoc_grpc_t *grpc, bson_t *reply)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT_PARAM (reply);

   // This function should only be invoked if `mongoc_grpc_handle_events()`
   // succeeded with a recv_message RPC event.
   BSON_ASSERT (grpc->reply);

   BSON_ASSERT (bson_steal (reply, grpc->reply));
   grpc->reply = NULL;
}


struct _op_tag_t {
   op_tag_t *next;
   op_tag_t *prev;

   grpc_op_type op;
   void *data;
};

typedef struct _recv_status_on_client_data_t {
   grpc_status_code status;
   grpc_slice status_details;
   const char *error_string;
   grpc_metadata_array trailing_metadata;
} recv_status_on_client_data_t;

typedef struct _send_initial_metadata_data_t {
   grpc_metadata *metadata;
   size_t count;
} send_initial_metadata_data_t;

typedef struct _recv_initial_metadata_data_t {
   grpc_metadata_array metadata;
} recv_initial_metadata_data_t;

typedef struct _send_message_data_t {
   void *compressed_data;
   size_t compressed_data_len;
   mongoc_iovec_t *iovecs;
   size_t num_iovecs;
   grpc_byte_buffer *send_message;
} send_message_data_t;

typedef struct _recv_message_data_t {
   grpc_byte_buffer *recv_message;
} recv_message_data_t;


static const char *
status_code_to_str (grpc_status_code status)
{
   switch (status) {
#define CASE_STATUS_CODE_TO_STR(STATUS) \
   case STATUS:                         \
      return #STATUS;

      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_OK)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_CANCELLED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_UNKNOWN)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_INVALID_ARGUMENT)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_DEADLINE_EXCEEDED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_NOT_FOUND)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_ALREADY_EXISTS)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_PERMISSION_DENIED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_UNAUTHENTICATED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_RESOURCE_EXHAUSTED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_FAILED_PRECONDITION)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_ABORTED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_OUT_OF_RANGE)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_UNIMPLEMENTED)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_INTERNAL)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_UNAVAILABLE)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS_DATA_LOSS)
      CASE_STATUS_CODE_TO_STR (GRPC_STATUS__DO_NOT_USE)
#undef CASE_OP_TYPE_TO_STR
   }

   MONGOC_WARNING ("unknown gRPC status code: %d", (int) status);
   return "unknown";
}

static bool
check_call_error (grpc_call_error call_error, bson_error_t *error)
{
   if (call_error != GRPC_CALL_OK) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_INVALID_STATE,
                      "gRPC call error: %s",
                      grpc_call_error_to_string (call_error));
      return false;
   }

   return true;
}

static mongoc_array_t
byte_buffer_to_array (grpc_byte_buffer *buffer)
{
   mongoc_array_t res;
   _mongoc_array_init (&res, sizeof (uint8_t));

   {
      grpc_byte_buffer_reader reader;
      grpc_byte_buffer_reader_init (&reader, buffer);

      for (grpc_slice slice;
           grpc_byte_buffer_reader_next (&reader, &slice) != 0;
           grpc_slice_unref (slice)) {
         const uint8_t *const data = GRPC_SLICE_START_PTR (slice);
         const size_t len = GRPC_SLICE_LENGTH (slice);
         BSON_ASSERT (bson_in_range_unsigned (uint32_t, len));
         _mongoc_array_append_vals (&res, data, (uint32_t) len);
      }

      grpc_byte_buffer_reader_destroy (&reader);
   }

   return res;
}

static send_message_data_t *
send_message_data_new (mongoc_grpc_t *grpc,
                       int32_t request_id,
                       uint32_t flags,
                       const bson_t *cmd,
                       const char *payload_identifier,
                       const uint8_t *payload,
                       int32_t payload_size,
                       int32_t compressor_id,
                       int32_t compression_level,
                       bson_error_t *error)
{
   send_message_data_t *ret = NULL;

   mcd_rpc_message *const rpc = grpc->rpc;
   mcd_rpc_message_reset (grpc->rpc);

   int32_t message_length = 0;

   message_length += mcd_rpc_header_set_message_length (rpc, 0);
   message_length += mcd_rpc_header_set_request_id (rpc, request_id);
   message_length += mcd_rpc_header_set_response_to (rpc, 0);
   message_length += mcd_rpc_header_set_op_code (rpc, MONGOC_OP_CODE_MSG);

   mcd_rpc_op_msg_set_sections_count (rpc, payload ? 2u : 1u);

   message_length += mcd_rpc_op_msg_set_flag_bits (rpc, flags);
   message_length += mcd_rpc_op_msg_section_set_kind (rpc, 0u, 0);
   message_length +=
      mcd_rpc_op_msg_section_set_body (rpc, 0u, bson_get_data (cmd));

   if (payload) {
      BSON_ASSERT (bson_in_range_signed (size_t, payload_size));

      const size_t section_length = sizeof (int32_t) +
                                    strlen (payload_identifier) + 1u +
                                    (size_t) payload_size;
      BSON_ASSERT (bson_in_range_unsigned (int32_t, section_length));

      message_length += mcd_rpc_op_msg_section_set_kind (rpc, 1u, 1);
      message_length +=
         mcd_rpc_op_msg_section_set_length (rpc, 1u, (int32_t) section_length);
      message_length +=
         mcd_rpc_op_msg_section_set_identifier (rpc, 1u, payload_identifier);
      message_length += mcd_rpc_op_msg_section_set_document_sequence (
         rpc, 1u, payload, (size_t) payload_size);
   }

   mcd_rpc_message_set_length (rpc, message_length);

   void *compressed_data = NULL;
   size_t compressed_data_len = 0u;

   // gRPC POC: Atlas Proxy does not support OP_COMPRESSED.
   // if (compressor_id != -1 && !mcd_rpc_message_compress (rpc,
   //                                                       compressor_id,
   //                                                       compression_level,
   //                                                       &compressed_data,
   //                                                       &compressed_data_len,
   //                                                       error)) {
   //    goto fail;
   // }

   size_t num_iovecs;
   mongoc_iovec_t *const iovecs = mcd_rpc_message_to_iovecs (rpc, &num_iovecs);
   BSON_ASSERT (iovecs);

   grpc_byte_buffer *const send_message =
      grpc_raw_byte_buffer_create (NULL, 0u);
   BSON_ASSERT (send_message);

   for (size_t i = 0u; i < num_iovecs; ++i) {
      grpc_slice_buffer_add (
         &send_message->data.raw.slice_buffer,
         grpc_slice_from_static_buffer (iovecs[i].iov_base, iovecs[i].iov_len));
   }

   ret = bson_malloc (sizeof (*ret));
   *ret = (send_message_data_t){
      .compressed_data = compressed_data, // Ownership transfer.
      .compressed_data_len = compressed_data_len,
      .iovecs = iovecs, // Ownership transfer.
      .num_iovecs = num_iovecs,
      .send_message = send_message, // Ownership transfer.
   };

   goto done;

fail:
   bson_free (ret);
   mongoc_grpc_call_cancel (grpc);

done:
   return ret;
}

static bool
recv_message_to_reply (mongoc_grpc_t *grpc,
                       grpc_byte_buffer *recv_message,
                       bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (recv_message || true);
   BSON_ASSERT (error || true);

   bool ret = true;

   // TODO: when does `recv_message == NULL` when `success != 0`?
   if (!recv_message) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "invalid reply from server: no response");
      goto fail;
   }

   mcd_rpc_message *const rpc = grpc->rpc;
   mcd_rpc_message_reset (grpc->rpc);

   mongoc_array_t bytes = byte_buffer_to_array (recv_message);

   if (!mcd_rpc_message_from_data_in_place (rpc, bytes.data, bytes.len, NULL)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "invalid reply from server: malformed message");
      goto fail;
   }

   mcd_rpc_message_ingress (rpc);

   void *decompressed_data = NULL;
   size_t decompressed_data_len = 0u;

   if (!mcd_rpc_message_decompress_if_necessary (
          grpc->rpc, &decompressed_data, &decompressed_data_len)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "invalid reply from server: decompression failure");
      goto fail;
   }

   bson_t body;
   if (!mcd_rpc_message_get_body (rpc, &body)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "invalid reply from server: malformed body");
      bson_free (decompressed_data);
      _mongoc_array_destroy (&bytes);
      goto fail;
   }

   bson_destroy (grpc->reply);
   grpc->reply = bson_copy (&body); // Unique ownership of all reply data.

   bson_destroy (&body);
   bson_free (decompressed_data);
   _mongoc_array_destroy (&bytes);

   goto done;

fail:
   ret = false;
   mongoc_grpc_call_cancel (grpc);

done:
   return ret;
}


static grpc_channel *
create_channel (void)
{
   // Authentication is not required for gRPC POC.
   grpc_channel_credentials *const creds =
      grpc_insecure_credentials_create (); // Experimental API!

   grpc_arg args[] = {
      {
         .key = GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH,
         .type = GRPC_ARG_INTEGER,
         .value.integer = max_message_size_bytes,
      },
      {
         .key = GRPC_ARG_MAX_SEND_MESSAGE_LENGTH,
         .type = GRPC_ARG_INTEGER,
         .value.integer = max_message_size_bytes,
      },
      {
         .key = GRPC_ARG_DEFAULT_AUTHORITY,
         .type = GRPC_ARG_STRING,
         .value.string = (char *) poc_atlas_authority,
      },
   };

   grpc_channel *const channel =
      grpc_channel_create (poc_atlas_target,
                           creds,
                           &(grpc_channel_args){
                              .args = args,
                              .num_args = sizeof (args) / sizeof (*args),
                           });

   grpc_channel_credentials_release (creds);

   return channel;
}


static grpc_call *
create_call (grpc_channel *channel, grpc_completion_queue *cq)
{
   BSON_ASSERT_PARAM (channel);
   BSON_ASSERT_PARAM (cq);

   return grpc_channel_create_call (
      channel,
      NULL,
      GRPC_PROPAGATE_DEFAULTS,
      cq,
      grpc_slice_from_static_string (poc_atlas_method),
      NULL,
      gpr_inf_future (GPR_CLOCK_REALTIME),
      NULL);
}

static void
send_close_from_client (grpc_call *call, bool ignore_call_error)
{
   BSON_ASSERT_PARAM (call);

   const grpc_call_error call_error =
      grpc_call_start_batch (call,
                             (grpc_op[]){
                                {
                                   .op = GRPC_OP_SEND_CLOSE_FROM_CLIENT,
                                   .flags = 0,
                                },
                             },
                             1u,
                             NULL,
                             NULL);

   (void) check_call_error (call_error, NULL);
}

static void
recv_status_on_client (grpc_call *call)
{
   recv_status_on_client_data_t *const data = bson_malloc (sizeof (*data));
   *data = (recv_status_on_client_data_t){
      .status = GRPC_STATUS_OK,
   };

   op_tag_t *const tag = bson_malloc (sizeof (*tag));
   *tag = (op_tag_t){
      .op = GRPC_OP_RECV_STATUS_ON_CLIENT,
      .data = data,
   };

   const grpc_call_error call_error = grpc_call_start_batch (
      call,
      (grpc_op[]){
         {
            .op = tag->op,
            .flags = 0,
            .data.recv_status_on_client.status = &data->status,
            .data.recv_status_on_client.status_details = &data->status_details,
            .data.recv_status_on_client.error_string = &data->error_string,
            .data.recv_status_on_client.trailing_metadata =
               &data->trailing_metadata,
         },
      },
      1u,
      tag,
      NULL);

   // recv_status_on_client always succeeds. Errors will be reported via the
   // status RPC when all activity on the call has completed (via call cancel or
   // the final unref).
   BSON_ASSERT (call_error == GRPC_CALL_OK);
}

static void
op_tag_destroy (op_tag_t *tag)
{
   if (!tag) {
      return;
   }

   BSON_ASSERT (tag->data);

   switch (tag->op) {
   case GRPC_OP_RECV_STATUS_ON_CLIENT: {
      recv_status_on_client_data_t *const data = tag->data;
      grpc_metadata_array_destroy (&data->trailing_metadata);
      grpc_slice_unref (data->status_details);
      gpr_free ((void *) data->error_string);
      bson_free (data);
      bson_free (tag);
      return;
   }

   case GRPC_OP_SEND_INITIAL_METADATA: {
      send_initial_metadata_data_t *const data = tag->data;
      gpr_free (data->metadata);
      bson_free (data);
      bson_free (tag);
      return;
   }

   case GRPC_OP_RECV_INITIAL_METADATA: {
      recv_initial_metadata_data_t *const data = tag->data;
      grpc_metadata_array_destroy (&data->metadata);
      bson_free (data);
      bson_free (tag);
      return;
   }

   case GRPC_OP_SEND_MESSAGE: {
      send_message_data_t *const data = tag->data;
      grpc_byte_buffer_destroy (data->send_message);
      bson_free (data->iovecs);
      bson_free (data->compressed_data);
      bson_free (data);
      bson_free (tag);
      return;
   }

   case GRPC_OP_RECV_MESSAGE: {
      recv_message_data_t *const data = tag->data;
      grpc_byte_buffer_destroy (data->recv_message);
      bson_free (data);
      bson_free (tag);
      return;
   }

   case GRPC_OP_SEND_CLOSE_FROM_CLIENT: {
      BSON_UNREACHABLE ("send_close_from_client should not have an event tag");
      return;
   }

   case GRPC_OP_SEND_STATUS_FROM_SERVER: {
      MONGOC_WARNING ("unexpected GRPC_OP_SEND_STATUS_FROM_SERVER");
      return;
   }

   case GRPC_OP_RECV_CLOSE_ON_SERVER: {
      MONGOC_WARNING ("unexpected GRPC_OP_RECV_CLOSE_ON_SERVER");
      return;
   }
   }

   MONGOC_WARNING ("invalid gRPC op type: %d", (int) tag->op);
   return;
}


static bool
start_send_initial_metadata (mongoc_grpc_t *grpc, bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (error || true);

   bool ret = true;

   grpc_metadata initial_metadata[] = {
#define METADATA_ENTRY(KEY, VALUE)                    \
   {                                                  \
      .key = grpc_slice_from_static_string (KEY),     \
      .value = grpc_slice_from_static_string (VALUE), \
   }
      // Most of these fields are hard-coded for gRPC POC.
      METADATA_ENTRY ("security-uuid", "uuid"),
      METADATA_ENTRY ("username", "user"),
      METADATA_ENTRY ("servername", "host.local.10gen.cc"),
      METADATA_ENTRY ("mongodb-wireversion", "18"),
      METADATA_ENTRY ("x-forwarded-for", "127.0.0.1:9901"),
#undef METADATA_ENTRY
   };

   send_initial_metadata_data_t *data = bson_malloc (sizeof (*data));
   *data = (send_initial_metadata_data_t){
      .metadata = gpr_malloc (sizeof (initial_metadata)),
      .count = sizeof (initial_metadata) / sizeof (*initial_metadata),
   };
   memmove (data->metadata, initial_metadata, sizeof (initial_metadata));

   grpc_metadata *const metadata = data->metadata;
   const size_t count = data->count;

   op_tag_t *const tag = bson_malloc (sizeof (*tag));
   *tag = (op_tag_t){
      .op = GRPC_OP_SEND_INITIAL_METADATA,
      .data = data, // Ownership transfer.
   };
   data = NULL;

   const grpc_call_error call_error = grpc_call_start_batch (
      grpc->call,
      (grpc_op[]){
         {
            .op = tag->op,
            .flags = 0,
            .data.send_initial_metadata.metadata = metadata,
            .data.send_initial_metadata.count = count,
         },
      },
      1u,
      tag, // Ownership transfer on success.
      NULL);

   if (!check_call_error (call_error, error)) {
      goto fail;
   }

   DL_APPEND (grpc->tags, tag);

   goto done;

fail:
   ret = false;
   op_tag_destroy (tag);
   mongoc_grpc_call_cancel (grpc);

done:
   bson_free (data);

   return ret;
}

static bool
start_recv_initial_metadata (mongoc_grpc_t *grpc, bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (error || true);

   bool ret = true;

   recv_initial_metadata_data_t *data = bson_malloc (sizeof (*data));
   grpc_metadata_array_init (&data->metadata);

   grpc_metadata_array *const metadata = &data->metadata;

   op_tag_t *const tag = bson_malloc (sizeof (*tag));
   *tag = (op_tag_t){
      .op = GRPC_OP_RECV_INITIAL_METADATA,
      .data = data, // Ownership transfer.
   };
   data = NULL;

   const grpc_call_error call_error = grpc_call_start_batch (
      grpc->call,
      (grpc_op[]){{
         .op = tag->op,
         .flags = 0,
         .data.recv_initial_metadata.recv_initial_metadata = metadata,
      }},
      1u,
      tag, // Ownership transfer on success.
      NULL);

   if (!check_call_error (call_error, error)) {
      goto fail;
   }

   DL_APPEND (grpc->tags, tag);

   goto done;

fail:
   ret = false;
   op_tag_destroy (tag);
   mongoc_grpc_call_cancel (grpc);

done:
   bson_free (data);

   return ret;
}


static bool
start_send_message (mongoc_grpc_t *grpc,
                    int32_t request_id,
                    uint32_t flags,
                    const bson_t *cmd,
                    const char *payload_identifier,
                    const uint8_t *payload,
                    int32_t payload_size,
                    int32_t compressor_id,
                    int32_t compression_level,
                    bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT_PARAM (cmd);
   BSON_ASSERT (payload_identifier || true);
   BSON_ASSERT (payload || true);
   BSON_ASSERT (error || true);

   bool ret = true;

   send_message_data_t *data = send_message_data_new (grpc,
                                                      request_id,
                                                      flags,
                                                      cmd,
                                                      payload_identifier,
                                                      payload,
                                                      payload_size,
                                                      compressor_id,
                                                      compression_level,
                                                      error);

   grpc_byte_buffer *const send_message = data ? data->send_message : NULL;

   op_tag_t *const tag = data ? bson_malloc (sizeof (*tag)) : NULL;
   if (tag) {
      *tag = (op_tag_t){
         .op = GRPC_OP_SEND_MESSAGE,
         .data = data, // Ownership transfer.
      };
      data = NULL;
   } else {
      goto fail;
   }

   const grpc_call_error call_error =
      grpc_call_start_batch (grpc->call,
                             (grpc_op[]){{
                                .op = tag->op,
                                .flags = 0,
                                .data.send_message.send_message = send_message,
                             }},
                             1u,
                             tag, // Ownership transfer on success.
                             NULL);

   if (!check_call_error (call_error, error)) {
      goto fail;
   }

   DL_APPEND (grpc->tags, tag);

   goto done;

fail:
   ret = false;
   op_tag_destroy (tag);
   mongoc_grpc_call_cancel (grpc);

done:
   bson_free (data);

   return ret;
}

static bool
start_recv_message (mongoc_grpc_t *grpc, bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (error || true);

   bool ret = true;

   recv_message_data_t *data = bson_malloc (sizeof (*data));
   *data = (recv_message_data_t){
      .recv_message = NULL,
   };

   grpc_byte_buffer **const recv_message = &data->recv_message;

   op_tag_t *const tag = bson_malloc (sizeof (*tag));
   *tag = (op_tag_t){
      .op = GRPC_OP_RECV_MESSAGE,
      .data = data, // Ownership transfer.
   };
   data = NULL;

   const grpc_call_error call_error =
      grpc_call_start_batch (grpc->call,
                             (grpc_op[]){{
                                .op = tag->op,
                                .flags = 0,
                                .data.recv_message.recv_message = recv_message,
                             }},
                             1u,
                             tag, // Ownership transfer on success.
                             NULL);

   if (!check_call_error (call_error, error)) {
      goto fail;
   }

   DL_APPEND (grpc->tags, tag);

   goto done;

fail:
   ret = false;
   op_tag_destroy (tag);
   mongoc_grpc_call_cancel (grpc);

done:
   return ret;
}

static bool
handle_event (mongoc_grpc_t *grpc, grpc_event event, bson_error_t *error)
{
   BSON_ASSERT_PARAM (grpc);
   BSON_ASSERT (error || true);

   bool ret = true;

   BSON_ASSERT (event.type == GRPC_OP_COMPLETE);
   BSON_ASSERT (event.tag);

   op_tag_t *const tag = event.tag;

   switch (tag->op) {
   case GRPC_OP_RECV_STATUS_ON_CLIENT: {
      recv_status_on_client_data_t *const data = tag->data;
      BSON_ASSERT (data);

      if (data->status != GRPC_STATUS_OK && error) {
         error->domain = MONGOC_ERROR_STREAM;
         error->code = MONGOC_ERROR_STREAM_INVALID_STATE;

         // Using error_string would be simpler, but the format of error_string
         // is too verbose for our needs.
         char *const details_str =
            grpc_slice_to_c_string (data->status_details);
         bson_snprintf (error->message,
                        sizeof (error->message),
                        "%s: %s",
                        status_code_to_str (data->status),
                        details_str);
         gpr_free (details_str);

         goto fail;
      }

      goto done;
   }

   case GRPC_OP_SEND_CLOSE_FROM_CLIENT: {
      DL_DELETE (grpc->tags, tag);
      goto done;
   }

   case GRPC_OP_SEND_INITIAL_METADATA: {
      DL_DELETE (grpc->tags, tag);
      goto done;
   }

   case GRPC_OP_RECV_INITIAL_METADATA: {
      DL_DELETE (grpc->tags, tag);
      // gRPC POC: we don't expect initial metadata from the server, and even if
      // we do receive any, there is nothing we need to do with it
      goto done;
   }

   case GRPC_OP_SEND_MESSAGE: {
      DL_DELETE (grpc->tags, tag);

      if (event.success != 0) {
         // Only increment egress on success (diverging from mongoRPC
         // implementation).
         mcd_rpc_message_egress (grpc->rpc);
      }

      goto done;
   }

   case GRPC_OP_RECV_MESSAGE: {
      DL_DELETE (grpc->tags, tag);

      recv_message_data_t *const data = tag->data;
      BSON_ASSERT (data);

      if (event.success == 0) {
         goto done;
      }

      if (!recv_message_to_reply (grpc, data->recv_message, error)) {
         goto fail;
      }

      goto done;
   }

   case GRPC_OP_SEND_STATUS_FROM_SERVER: {
      BSON_UNREACHABLE ("unexpected GRPC_OP_SEND_STATUS_FROM_SERVER");
      return false;
   }

   case GRPC_OP_RECV_CLOSE_ON_SERVER: {
      BSON_UNREACHABLE ("unexpected GRPC_OP_RECV_CLOSE_ON_SERVER");
      return false;
   }
   }

   BSON_UNREACHABLE ("invalid op type");

fail:
   ret = false;
   mongoc_grpc_call_cancel (grpc);

done:
   op_tag_destroy (tag);

   return ret;
}

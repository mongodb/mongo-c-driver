#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h"

#include <mongoc/mcd-rpc.h>
#include <mongoc/mongoc-grpc-private.h>
#include <mongoc/mongoc-compression-private.h>

#ifndef MONGOC_ENABLE_SHM_COUNTERS
#error "gRPC POC tests require ENABLE_SHM_COUNTERS=ON"
#endif
#include <mongoc/mongoc-counters-private.h>

#include <grpc/support/time.h>

#define INFINITE_DEADLINE (gpr_inf_future (GPR_CLOCK_REALTIME))
#define IMMEDIATE_DEADLINE (gpr_now (GPR_CLOCK_REALTIME))
#define DEFAULT_DEADLINE            \
   (gpr_time_add (                  \
      gpr_now (GPR_CLOCK_REALTIME), \
      gpr_time_from_millis (get_future_timeout_ms (), GPR_TIMESPAN)))

#define TMP_BSON_FROM_RAW(...) tmp_bson (BSON_STR (__VA_ARGS__))

#define ASSERT_CONNECTIVITY_STATE(state)                      \
   if (1) {                                                   \
      const grpc_connectivity_state expected_state = (state); \
      const grpc_connectivity_state actual_state =            \
         mongoc_grpc_check_connectivity_state (grpc);         \
      ASSERT_WITH_MSG (expected_state == actual_state,        \
                       "expected %d, got %d",                 \
                       (int) expected_state,                  \
                       (int) actual_state);                   \
   } else                                                     \
      (void) 0

#define ASSERT_COUNTERS(expected_op_egress_msg,                                \
                        expected_op_ingress_msg,                               \
                        expected_op_egress_compressed,                         \
                        expected_op_ingress_compressed)                        \
   if (1) {                                                                    \
      const int32_t expected_op_egress_total =                                 \
         expected_op_egress_msg + expected_op_egress_compressed;               \
      const int32_t expected_op_ingress_total =                                \
         expected_op_ingress_msg + expected_op_ingress_compressed;             \
      const int32_t actual_op_egress_msg =                                     \
         mongoc_counter_op_egress_msg_count ();                                \
      const int32_t actual_op_ingress_msg =                                    \
         mongoc_counter_op_ingress_msg_count ();                               \
      const int32_t actual_op_egress_compressed =                              \
         mongoc_counter_op_egress_compressed_count ();                         \
      const int32_t actual_op_ingress_compressed =                             \
         mongoc_counter_op_ingress_compressed_count ();                        \
      const int32_t actual_op_egress_query =                                   \
         mongoc_counter_op_egress_query_count ();                              \
      const int32_t actual_op_ingress_reply =                                  \
         mongoc_counter_op_ingress_reply_count ();                             \
      const int32_t actual_op_egress_total =                                   \
         mongoc_counter_op_egress_total_count ();                              \
      const int32_t actual_op_ingress_total =                                  \
         mongoc_counter_op_ingress_total_count ();                             \
      ASSERT_WITH_MSG (actual_op_egress_msg == expected_op_egress_msg,         \
                       "op_egress_msg: expected %" PRId32 ", got %" PRId32,    \
                       expected_op_egress_msg,                                 \
                       actual_op_egress_msg);                                  \
      ASSERT_WITH_MSG (actual_op_ingress_msg == expected_op_ingress_msg,       \
                       "op_ingress_msg: expected %" PRId32 ", got %" PRId32,   \
                       expected_op_ingress_msg,                                \
                       actual_op_ingress_msg);                                 \
      ASSERT_WITH_MSG (                                                        \
         actual_op_egress_compressed == expected_op_egress_compressed,         \
         "op_egress_compressed: expected %" PRId32 ", got %" PRId32,           \
         expected_op_egress_compressed,                                        \
         actual_op_egress_compressed);                                         \
      ASSERT_WITH_MSG (                                                        \
         actual_op_ingress_compressed == expected_op_ingress_compressed,       \
         "op_ingress_compressed: expected %" PRId32 ", got %" PRId32,          \
         expected_op_ingress_compressed,                                       \
         actual_op_ingress_compressed);                                        \
      /* gRPC POC: should always be zero. */                                   \
      ASSERT_WITH_MSG (actual_op_egress_query == 0,                            \
                       "op_egress_query: expected %" PRId32 ", got %" PRId32,  \
                       0,                                                      \
                       actual_op_egress_query);                                \
      ASSERT_WITH_MSG (actual_op_ingress_reply == 0,                           \
                       "op_ingress_reply: expected %" PRId32 ", got %" PRId32, \
                       0,                                                      \
                       actual_op_ingress_reply);                               \
      ASSERT_WITH_MSG (actual_op_egress_total == expected_op_egress_total,     \
                       "op_egress_total: expected %" PRId32 ", got %" PRId32,  \
                       expected_op_egress_total,                               \
                       actual_op_egress_total);                                \
      ASSERT_WITH_MSG (actual_op_ingress_total == expected_op_ingress_total,   \
                       "op_ingress_total: expected %" PRId32 ", got %" PRId32, \
                       expected_op_ingress_total,                              \
                       actual_op_ingress_total);                               \
   } else                                                                      \
      (void) 0

#define ASSERT_REPLY_OK()                                       \
   if (1) {                                                     \
      bson_t reply;                                             \
      bson_iter_t iter;                                         \
      mongoc_grpc_steal_reply (grpc, &reply);                   \
      ASSERT (bson_iter_init_find (&iter, &reply, "ok"));       \
      if (bson_iter_int32 (&iter) != 1) {                       \
         char *const json = bson_as_json (&reply, NULL);        \
         ASSERT_WITH_MSG (false, "unexpected reply: %s", json); \
         bson_free (json);                                      \
      }                                                         \
      bson_destroy (&reply);                                    \
   } else                                                       \
      (void) 0

// gRPC POC: hard-coded constants specific to gRPC POC or Atlas Proxy.
static const char *const poc_atlas_target = "host9.local.10gen.cc:9901";
static const char *const poc_atlas_legacy = "host9.local.10gen.cc:9900";

static mongoc_grpc_t *
_grpc_new_with_target (const char *target)
{
   capture_logs (true);
   mongoc_grpc_t *const ret = mongoc_grpc_new (target);
   capture_logs (false);
   return ret;
}

static mongoc_grpc_t *
_grpc_new (void)
{
   return _grpc_new_with_target (poc_atlas_target);
}

static void
_reset_counters (void)
{
   mongoc_counter_op_egress_msg_reset ();
   mongoc_counter_op_ingress_msg_reset ();
   mongoc_counter_op_egress_compressed_reset ();
   mongoc_counter_op_ingress_compressed_reset ();
   mongoc_counter_op_egress_query_reset ();
   mongoc_counter_op_ingress_reply_reset ();
   mongoc_counter_op_egress_total_reset ();
   mongoc_counter_op_ingress_total_reset ();
}

static void
test_grpc_poc_warning (void)
{
   capture_logs (true);

   {
      mongoc_grpc_t *const grpc = mongoc_grpc_new (poc_atlas_target);
      mongoc_grpc_destroy (grpc);
      ASSERT_NO_CAPTURED_LOGS ("valid gRPC POC target should not emit warning");
   }

   clear_captured_logs ();

   {
      mongoc_grpc_t *const grpc = mongoc_grpc_new (poc_atlas_legacy);
      mongoc_grpc_destroy (grpc);
      ASSERT_CAPTURED_LOG ("expected gRPC POC warning for unexpected target",
                           MONGOC_LOG_LEVEL_WARNING,
                           "gRPC POC");
   }

   capture_logs (false);
}

static void
test_grpc_new (void)
{
   mongoc_grpc_t *const grpc = _grpc_new ();

   bson_error_t error = {.code = 0};

   _reset_counters ();

   // No RPC events should be submitted on creation.
   ASSERT_OR_PRINT (
      mongoc_grpc_handle_events (grpc, IMMEDIATE_DEADLINE, &error), error);

   // No RPC events == no connection attempt.
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_IDLE);

   // No RPC events == no timeout error.
   ASSERT (!mongoc_grpc_event_timed_out (grpc));

   mongoc_grpc_destroy (grpc);

   ASSERT_COUNTERS (0, 0, 0, 0);
}

static void
test_grpc_initial_metadata (void)
{
   _reset_counters ();

   {
      mongoc_grpc_t *const grpc = _grpc_new ();

      bson_error_t error = {.code = 0};

      // No reason to expect an error when sending initial metadata.
      ASSERT_OR_PRINT (mongoc_grpc_start_initial_metadata (grpc, &error),
                       error);

      // send_initial_metadata RPC event triggers a connection attempt but
      // does not send any messages yet.
      ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_CONNECTING);

      // Initial metadata RPC events will always timeout due to no attempt
      // yet to send or receive any messages.
      ASSERT (!mongoc_grpc_handle_events (grpc, IMMEDIATE_DEADLINE, &error));
      ASSERT (mongoc_grpc_event_timed_out (grpc));

      // Timeouts do not affect channel state.
      ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_CONNECTING);

      mongoc_grpc_destroy (grpc);
      ASSERT_COUNTERS (0, 0, 0, 0);
   }

   {
      mongoc_grpc_t *const grpc = _grpc_new ();

      bson_error_t error = {.code = 0};

      ASSERT_OR_PRINT (mongoc_grpc_start_initial_metadata (grpc, &error),
                       error);

      // Only one send_initial_metadata RPC should ever be started.
      ASSERT (!mongoc_grpc_start_initial_metadata (grpc, &error));
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_STREAM,
                             MONGOC_ERROR_STREAM_INVALID_STATE,
                             "GRPC_CALL_ERROR_TOO_MANY_OPERATIONS");

      mongoc_grpc_destroy (grpc);
      ASSERT_COUNTERS (0, 0, 0, 0);
   }
}

static void
test_grpc_message (void)
{
   mongoc_grpc_t *const grpc = _grpc_new ();

   bson_error_t error = {.code = 0};

   _reset_counters ();

   ASSERT_OR_PRINT (mongoc_grpc_start_initial_metadata (grpc, &error), error);

   // Send a hello to trigger the connection attempt.
   ASSERT_OR_PRINT (
      mongoc_grpc_start_message (grpc,
                                 0,
                                 MONGOC_OP_MSG_FLAG_NONE,
                                 tmp_bson ("{'hello': 1, '$db': 'admin'}"),
                                 -1,
                                 -1,
                                 &error),
      error);

   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_READY);
   ASSERT_COUNTERS (1, 1, 0, 0);
   ASSERT_REPLY_OK ();

   // There should be no more events to handle.
   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_READY);
   ASSERT_COUNTERS (1, 1, 0, 0);

   mongoc_grpc_destroy (grpc);
   ASSERT_COUNTERS (1, 1, 0, 0);
}

static void
test_grpc_message_compressed (void)
{
   mongoc_grpc_t *const grpc = _grpc_new ();

   bson_error_t error = {.code = 0};

   _reset_counters ();

   ASSERT_OR_PRINT (mongoc_grpc_start_initial_metadata (grpc, &error), error);

#ifndef MONGOC_ENABLE_COMPRESSION_ZLIB
#error "gRPC POC tests require ENABLE_ZLIB=GRPC"
#endif

   // Send a hello to trigger the connection attempt.
   ASSERT_OR_PRINT (
      mongoc_grpc_start_message (grpc,
                                 0,
                                 MONGOC_OP_MSG_FLAG_NONE,
                                 tmp_bson ("{'hello': 1, '$db': 'admin'}"),
                                 MONGOC_COMPRESSOR_ZLIB_ID,
                                 -1,
                                 &error),
      error);

   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_READY);
   // gRPC POC: Atlas Proxy does not support OP_COMPRESSED.
   // ASSERT_COUNTERS (1, 1, 1, 1);
   ASSERT_REPLY_OK ();

   // There should be no more events to handle.
   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_READY);
   // gRPC POC: Atlas Proxy does not support OP_COMPRESSED.
   // ASSERT_COUNTERS (1, 1, 1, 1);

   mongoc_grpc_destroy (grpc);
   // gRPC POC: Atlas Proxy does not support OP_COMPRESSED.
   // ASSERT_COUNTERS (1, 1, 1, 1);
}

static void
test_grpc_legacy_error (void)
{
   mongoc_grpc_t *const grpc = _grpc_new_with_target (poc_atlas_legacy);

   bson_error_t error = {.code = 0};

   _reset_counters ();

   ASSERT_OR_PRINT (mongoc_grpc_start_initial_metadata (grpc, &error), error);

   // Send a ping to trigger a response from the server.
   ASSERT_OR_PRINT (
      mongoc_grpc_start_message (grpc,
                                 0,
                                 MONGOC_OP_MSG_FLAG_NONE,
                                 tmp_bson ("{'ping': 1, '$db': 'admin'}"),
                                 -1,
                                 -1,
                                 &error),
      error);

   // Connection should succeed and server should reply with an error
   // message.
   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_READY);
   ASSERT_COUNTERS (1, 1, 0, 0);
   ASSERT_REPLY_OK ();

   // There should be no more events to handle.
   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_READY);
   ASSERT_COUNTERS (1, 1, 0, 0);

   mongoc_grpc_destroy (grpc);
   ASSERT_COUNTERS (1, 1, 0, 0);
}

static void
test_grpc_call_cancel (void)
{
   mongoc_grpc_t *const grpc = _grpc_new ();
   bson_error_t error = {.code = 0};

   _reset_counters ();

   mongoc_grpc_call_cancel (grpc);

   // Starting new RPC events after a call cancel is OK.
   ASSERT_OR_PRINT (mongoc_grpc_start_initial_metadata (grpc, &error), error);

   ASSERT (!mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error));
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_STREAM,
                          MONGOC_ERROR_STREAM_INVALID_STATE,
                          "CANCELLED");

   // An unsuccessful send_initial_metadata RPC event should not trigger a
   // connection attempt.
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_IDLE);

   // An unsuccessful send_message RPC event should not incremented the OP_MSG
   // egress counter (diverges from mongoRPC behavior).
   ASSERT_OR_PRINT (
      mongoc_grpc_start_message (grpc,
                                 0,
                                 MONGOC_OP_MSG_FLAG_NONE,
                                 tmp_bson ("{'ping': 1, '$db': 'admin'}"),
                                 -1,
                                 -1,
                                 &error),
      error);
   ASSERT_COUNTERS (0, 0, 0, 0);

   // There should be no more events to handle.
   ASSERT_OR_PRINT (mongoc_grpc_handle_events (grpc, DEFAULT_DEADLINE, &error),
                    error);
   ASSERT (!mongoc_grpc_event_timed_out (grpc));
   ASSERT_CONNECTIVITY_STATE (GRPC_CHANNEL_IDLE);

   mongoc_grpc_destroy (grpc);
}

void
test_grpc_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/grpc/poc-warning", test_grpc_poc_warning);
   TestSuite_Add (suite, "/grpc/new", test_grpc_new);
   TestSuite_Add (suite, "/grpc/initial_metadata", test_grpc_initial_metadata);
   TestSuite_Add (suite, "/grpc/message", test_grpc_message);
   TestSuite_Add (
      suite, "/grpc/message_compressed", test_grpc_message_compressed);
   TestSuite_Add (suite, "/grpc/legacy_error", test_grpc_legacy_error);
   TestSuite_Add (suite, "/grpc/call_cancel", test_grpc_call_cancel);
}

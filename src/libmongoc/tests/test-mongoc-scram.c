#include <mongoc.h>
#include <mock_server/mock-server.h>
#include <mock_server/future.h>
#include <mock_server/future-functions.h>

#include "mongoc-crypto-private.h"
#include "mongoc-scram-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"

#ifdef MONGOC_ENABLE_SSL
static void
test_mongoc_scram_step_username_not_set (void)
{
   mongoc_scram_t scram;
   bool success;
   uint8_t buf[4096] = {0};
   uint32_t buflen = 0;
   bson_error_t error;

   _mongoc_scram_init (&scram, MONGOC_CRYPTO_ALGORITHM_SHA_1);
   _mongoc_scram_set_pass (&scram, "password");

   success = _mongoc_scram_step (
      &scram, buf, buflen, buf, sizeof buf, &buflen, &error);

   ASSERT (!success);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SCRAM,
                          MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                          "SCRAM Failure: username is not set");

   _mongoc_scram_destroy (&scram);
}

typedef struct {
   const char *original;
   const char *normalized;
   bool should_be_required;
   bool should_succeed;
} sasl_prep_testcase_t;


/* test that an error is reported if the server responds with an iteration
 * count that is less than 4096 */
static void
test_iteration_count (int count, bool should_succeed)
{
   mongoc_scram_t scram;
   uint8_t buf[4096] = {0};
   uint32_t buflen = 0;
   bson_error_t error;
   const char *client_nonce = "YWJjZA==";
   char *server_response;
   bool success;

   server_response = bson_strdup_printf (
      "r=YWJjZA==YWJjZA==,s=r6+P1iLmSJvhrRyuFi6Wsg==,i=%d", count);
   /* set up the scram state to immediately test step 2. */
   _mongoc_scram_init (&scram, MONGOC_CRYPTO_ALGORITHM_SHA_1);
   _mongoc_scram_set_pass (&scram, "password");
   memcpy (scram.encoded_nonce, client_nonce, sizeof (scram.encoded_nonce));
   scram.encoded_nonce_len = (int32_t) strlen (client_nonce);
   scram.auth_message = bson_malloc0 (4096);
   scram.auth_messagemax = 4096;
   /* prepare the server's "response" from step 1 as the input for step 2. */
   memcpy (buf, server_response, strlen (server_response) + 1);
   buflen = (int32_t) strlen (server_response);
   scram.step = 1;
   success = _mongoc_scram_step (
      &scram, buf, buflen, buf, sizeof buf, &buflen, &error);
   if (should_succeed) {
      ASSERT_OR_PRINT (success, error);
   } else {
      BSON_ASSERT (!success);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_SCRAM,
                             MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                             "SCRAM Failure: iterations must be at least 4096");
   }
   bson_free (server_response);
   _mongoc_scram_destroy (&scram);
}

static void
test_mongoc_scram_iteration_count (void)
{
   test_iteration_count (1000, false);
   test_iteration_count (4095, false);
   test_iteration_count (4096, true);
   test_iteration_count (10000, true);
}

static void
test_mongoc_scram_sasl_prep (void)
{
#ifdef MONGOC_ENABLE_ICU
   int i, ntests;
   char *normalized;
   bson_error_t err;
   /* examples from RFC 4013 section 3. */
   sasl_prep_testcase_t tests[] = {{"\x65\xCC\x81", "\xC3\xA9", true, true},
                                   {"I\xC2\xADX", "IX", true, true},
                                   {"user", "user", false, true},
                                   {"USER", "USER", false, true},
                                   {"\xC2\xAA", "a", true, true},
                                   {"\xE2\x85\xA8", "IX", true, true},
                                   {"\x07", "(invalid)", true, false},
                                   {"\xD8\xA7\x31", "(invalid)", true, false}};
   ntests = sizeof (tests) / sizeof (sasl_prep_testcase_t);
   for (i = 0; i < ntests; i++) {
      ASSERT_CMPINT (tests[i].should_be_required,
                     ==,
                     _mongoc_sasl_prep_required (tests[i].original));
      memset (&err, 0, sizeof (err));
      normalized = _mongoc_sasl_prep (
         tests[i].original, strlen (tests[i].original), &err);
      if (tests[i].should_succeed) {
         ASSERT_CMPSTR (tests[i].normalized, normalized);
         ASSERT_CMPINT (err.code, ==, 0);
         bson_free (normalized);
      } else {
         ASSERT_CMPINT (err.code, ==, MONGOC_ERROR_SCRAM_PROTOCOL_ERROR);
         ASSERT_CMPINT (err.domain, ==, MONGOC_ERROR_SCRAM);
         BSON_ASSERT (normalized == NULL);
      }
   }
#endif
}
#endif

static void
_create_scram_users (void)
{
   mongoc_client_t *client;
   bool res;
   bson_error_t error;
   client = test_framework_client_new ();
   res = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'createUser': 'sha1', 'pwd': 'sha1', 'roles': ['root'], "
                "'mechanisms': ['SCRAM-SHA-1']}"),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'createUser': 'sha256', 'pwd': 'sha256', 'roles': ['root'], "
                "'mechanisms': ['SCRAM-SHA-256']}"),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'createUser': 'both', 'pwd': 'both', 'roles': ['root'], "
                "'mechanisms': ['SCRAM-SHA-1', 'SCRAM-SHA-256']}"),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);
   ASSERT_OR_PRINT (res, error);
   mongoc_client_destroy (client);
}

static void
_drop_scram_users (void)
{
   mongoc_client_t *client;
   mongoc_database_t *db;
   bool res;
   bson_error_t error;
   client = test_framework_client_new ();
   db = mongoc_client_get_database (client, "admin");
   res = mongoc_database_remove_user (db, "sha1", &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_database_remove_user (db, "sha256", &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_database_remove_user (db, "both", &error);
   ASSERT_OR_PRINT (res, error);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);
}

static void
_check_mechanism (const char *user,
                  const char *pwd,
                  const char *mechanism,
                  const char *mechanism_expected)
{
   mock_server_t *server;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   future_t *future;
   request_t *request;
   const bson_t *sasl_doc;
   const char *mechanism_used;

   server = mock_server_with_autoismaster (WIRE_VERSION_MAX);
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_username (uri, user);
   mongoc_uri_set_password (uri, pwd);
   if (mechanism) {
      mongoc_uri_set_auth_mechanism (uri, mechanism);
   }

   client = mongoc_client_new_from_uri (uri);
   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'dbstats': 1}"),
                                          NULL /* read_prefs. */,
                                          NULL /* reply. */,
                                          NULL /* error. */);
   request =
      mock_server_receives_msg (server, MONGOC_QUERY_NONE, tmp_bson ("{}"));
   sasl_doc = request_get_doc (request, 0);
   mechanism_used = bson_lookup_utf8 (sasl_doc, "mechanism");
   ASSERT_CMPSTR (mechanism_used, mechanism_expected);
   /* we're not actually going to auth, just hang up. */
   mock_server_hangs_up (request);
   future_wait (future);
   future_destroy (future);
   request_destroy (request);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

static void
_try_auth (const char *user,
           const char *pwd,
           const char *mechanism,
           bool should_succeed)
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t reply;
   bool res;

   uri = test_framework_get_uri ();
   mongoc_uri_set_username (uri, user);
   mongoc_uri_set_password (uri, pwd);
   if (mechanism) {
      mongoc_uri_set_auth_mechanism (uri, mechanism);
   }
   client = mongoc_client_new_from_uri (uri);
   mongoc_client_set_error_api (client, 2);
   res = mongoc_client_command_simple (client,
                                       "admin",
                                       tmp_bson ("{'dbstats': 1}"),
                                       NULL /* read_prefs. */,
                                       &reply,
                                       &error);
   if (should_succeed) {
      ASSERT_OR_PRINT (res, error);
      ASSERT_MATCH (&reply, "{'db': 'admin', 'ok': 1}");
   } else {
      ASSERT (!res);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "Authentication failed");
   }
   bson_destroy (&reply);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
}

/* test the auth tests described in the auth spec. */
static void
test_mongoc_scram_auth (void *ctx)
{
   /* Auth spec: "Create three test users, one with only SHA-1, one with only
    * SHA-256 and one with both" */
   _create_scram_users ();
   /* Auth spec: "For each test user, verify that you can connect and run a
      command requiring authentication for the following cases:
      - Explicitly specifying each mechanism the user supports.
      - Specifying no mechanism and relying on mechanism negotiation."
   */
   _try_auth ("sha1", "sha1", NULL, true);
   _try_auth ("sha1", "sha1", "SCRAM-SHA-1", true);
   _try_auth ("sha256", "sha256", "SCRAM-SHA-256", true);
   _try_auth ("both", "both", NULL, true);
   _try_auth ("both", "both", "SCRAM-SHA-1", true);
   _try_auth ("both", "both", "SCRAM-SHA-256", true);

   /* Auth spec: "For a test user supporting both SCRAM-SHA-1 and SCRAM-SHA-256,
    * drivers should verify that negotiation selects SCRAM-SHA-256" */
   /* TODO: CDRIVER-2579, after mechanism is negotiated, SCRAM-SHA-256 should be
    * the default:
    * _check_mechanism ("sha256", "sha256", NULL, "SCRAM-SHA-256");
    * _try_auth ("sha256", "sha256", NULL, true);
    */
   _check_mechanism ("sha1", "sha1", NULL, "SCRAM-SHA-1");
   _check_mechanism ("both", "both", NULL, "SCRAM-SHA-1");
   _check_mechanism ("both", "both", "SCRAM-SHA-1", "SCRAM-SHA-1");
   _check_mechanism ("both", "both", "SCRAM-SHA-256", "SCRAM-SHA-256");

   /* Test some failure auths. */
   _try_auth ("sha1", "bad", NULL, false);
   _try_auth ("sha256", "bad", NULL, false);
   _try_auth ("both", "bad", NULL, false);
   _try_auth ("sha1", "bad", "SCRAM-SHA-256", false);
   _try_auth ("sha256", "bad", "SCRAM-SHA-1", false);
   _drop_scram_users ();
}

static int
_skip_if_no_sha256 ()
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   bool res;

   uri = test_framework_get_uri ();
   mongoc_uri_set_auth_mechanism (uri, "SCRAM-SHA-256");
   client = mongoc_client_new_from_uri (uri);
   res = mongoc_client_command_simple (client,
                                       "admin",
                                       tmp_bson ("{'dbstats': 1}"),
                                       NULL /* read_prefs */,
                                       NULL /* reply */,
                                       NULL /* error */);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   return res ? 1 : 0;
}

void
test_scram_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_SSL
   TestSuite_Add (suite,
                  "/scram/username_not_set",
                  test_mongoc_scram_step_username_not_set);
   TestSuite_Add (suite, "/scram/sasl_prep", test_mongoc_scram_sasl_prep);
   TestSuite_Add (
      suite, "/scram/iteration_count", test_mongoc_scram_iteration_count);
#endif
   TestSuite_AddFull (suite,
                      "/scram/auth_tests",
                      test_mongoc_scram_auth,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_no_auth,
                      test_framework_skip_if_max_wire_version_less_than_6,
                      _skip_if_no_sha256,
                      TestSuite_CheckLive);
}

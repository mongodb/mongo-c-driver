#include <mongoc/mongoc.h>
#include <mongoc/mongoc-ssl-private.h>

#ifdef MONGOC_ENABLE_SSL_OPENSSL
#include <mongoc/mongoc-openssl-private.h>
#endif

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h" // tmp_bson

#ifdef MONGOC_ENABLE_OCSP_OPENSSL
/* Test parsing a DER encoded tlsfeature extension contents for the
 * status_request (value 5). This is a SEQUENCE of INTEGER. libmongoc assumes
 * this is a sequence of one byte integers. */

static void
_expect_malformed (const char *data, int32_t len)
{
   bool ret;

   ret = _mongoc_tlsfeature_has_status_request ((const uint8_t *) data, len);
   BSON_ASSERT (!ret);
   ASSERT_CAPTURED_LOG ("mongoc", MONGOC_LOG_LEVEL_ERROR, "malformed");
   clear_captured_logs ();
}

static void
_expect_no_status_request (const char *data, int32_t len)
{
   bool ret;
   ret = _mongoc_tlsfeature_has_status_request ((const uint8_t *) data, len);
   BSON_ASSERT (!ret);
   ASSERT_NO_CAPTURED_LOGS ("mongoc");
}

static void
_expect_status_request (const char *data, int32_t len)
{
   bool ret;
   ret = _mongoc_tlsfeature_has_status_request ((const uint8_t *) data, len);
   BSON_ASSERT (ret);
   ASSERT_NO_CAPTURED_LOGS ("mongoc");
}

static void
test_tlsfeature_parsing (void)
{
   capture_logs (true);
   /* A sequence of one integer = 5. */
   _expect_status_request ("\x30\x03\x02\x01\x05", 5);
   /* A sequence of one integer = 6. */
   _expect_no_status_request ("\x30\x03\x02\x01\x06", 5);
   /* A sequence of two integers = 5,6. */
   _expect_status_request ("\x30\x03\x02\x01\x05\x02\x01\x06", 8);
   /* A sequence of two integers = 6,5. */
   _expect_status_request ("\x30\x03\x02\x01\x06\x02\x01\x05", 8);
   /* A sequence containing a non-integer. Parsing fails. */
   _expect_malformed ("\x30\x03\x03\x01\x05\x02\x01\x06", 8);
   /* A non-sequence. It will not read past the first byte (despite the >1
    * length). */
   _expect_malformed ("\xFF", 2);
   /* A sequence with a length represented in more than one byte. Parsing fails.
    */
   _expect_malformed ("\x30\x82\x04\x48", 4);
   /* An integer with length > 1. Parsing fails. */
   _expect_malformed ("\x30\x03\x02\x02\x05\x05", 6);
}
#endif /* MONGOC_ENABLE_OCSP_OPENSSL */

#ifdef MONGOC_ENABLE_SSL
static void
create_x509_user (void)
{
   bson_error_t error;

   mongoc_client_t *client = test_framework_new_default_client ();
   bool ok =
      mongoc_client_command_simple (client,
                                    "$external",
                                    tmp_bson (BSON_STR ({
                                       "createUser" : "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client",
                                       "roles" : [ {"role" : "readWrite", "db" : "db"} ]
                                    })),
                                    NULL /* read_prefs */,
                                    NULL /* reply */,
                                    &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_client_destroy (client);
}

static void
drop_x509_user (bool ignore_notfound)
{
   bson_error_t error;

   mongoc_client_t *client = test_framework_new_default_client ();
   bool ok = mongoc_client_command_simple (
      client,
      "$external",
      tmp_bson (BSON_STR ({"dropUser" : "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client"})),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);

   if (!ok) {
      ASSERT_OR_PRINT (ignore_notfound && NULL != strstr (error.message, "not found"), error);
   }
   mongoc_client_destroy (client);
}

static mongoc_uri_t *
get_x509_uri (void)
{
   bson_error_t error;
   char *uristr_noauth = test_framework_get_uri_str_no_auth ("db");
   mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr_noauth, &error);
   ASSERT_OR_PRINT (uri, error);
   ASSERT (mongoc_uri_set_auth_mechanism (uri, "MONGODB-X509"));
   ASSERT (mongoc_uri_set_auth_source (uri, "$external"));
   bson_free (uristr_noauth);
   return uri;
}

static bool
try_insert (mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
   bool ok = mongoc_collection_insert_one (coll, tmp_bson ("{}"), NULL, NULL, error);
   mongoc_collection_destroy (coll);
   return ok;
}

static void
test_x509_auth (void *unused)
{
   drop_x509_user (true /* ignore "not found" error */);
   create_x509_user ();

   // Test auth works:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_CLIENT));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
         ASSERT_OR_PRINT (client, error);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT_OR_PRINT (ok, error);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails with no client certificate:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
         ASSERT_OR_PRINT (client, error);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
      mongoc_uri_destroy (uri);
   }

   // Test auth works with explicit username:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_username (uri, "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_CLIENT));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
         ASSERT_OR_PRINT (client, error);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT_OR_PRINT (ok, error);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails with wrong username:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_username (uri, "bad"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_CLIENT));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
         ASSERT_OR_PRINT (client, error);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails with correct username but wrong certificate:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_username (uri, "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_SERVER));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
         ASSERT_OR_PRINT (client, error);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
      mongoc_uri_destroy (uri);
   }
   drop_x509_user (false);
}
#endif // MONGOC_ENABLE_SSL

void
test_x509_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_SSL
   TestSuite_AddFull (suite, "/X509/auth", test_x509_auth, NULL, NULL, test_framework_skip_if_no_auth);
#endif

#ifdef MONGOC_ENABLE_OCSP_OPENSSL
   TestSuite_Add (suite, "/X509/tlsfeature_parsing", test_tlsfeature_parsing);
#endif
}

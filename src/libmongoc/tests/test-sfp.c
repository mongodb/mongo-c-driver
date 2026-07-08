#include <mongoc/mongoc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
   SFP_AUTH_UNAUTHENTICATED,
   SFP_AUTH_SCRAM,
   SFP_AUTH_X509,
} sfp_auth_t;

typedef enum {
   SFP_VARIANT_BASELINE,
   SFP_VARIANT_COMPRESSED,
   SFP_VARIANT_SERVER_API,
} sfp_variant_t;

static const char *
sfp_auth_to_string(sfp_auth_t auth)
{
   switch (auth) {
   case SFP_AUTH_UNAUTHENTICATED:
      return "unauthenticated";
   case SFP_AUTH_SCRAM:
      return "scram";
   case SFP_AUTH_X509:
      return "x509";
   default:
      return "unknown";
   }
}

static const char *
sfp_variant_to_string(sfp_variant_t variant)
{
   switch (variant) {
   case SFP_VARIANT_BASELINE:
      return "baseline";
   case SFP_VARIANT_COMPRESSED:
      return "compressed";
   case SFP_VARIANT_SERVER_API:
      return "server_api";
   default:
      return "unknown";
   }
}

static char *
_getenv_required(const char *name)
{
#ifdef _MSC_VER
   char buf[1024] = {0};
   size_t buflen;
   if ((0 == getenv_s(&buflen, buf, sizeof buf, name)) && strlen(buf) > 0) {
      return bson_strdup(buf);
   }
   fprintf(stderr, "Required environment variable %s is not set\n", name);
   abort();
#else
   char *const value = getenv(name);
   if (value && strlen(value)) {
      return bson_strdup(value);
   }
   fprintf(stderr, "Required environment variable %s is not set\n", name);
   abort();
#endif
}

typedef struct {
   mongoc_client_t *client;
   mongoc_collection_t *coll;
} test_sfp_t;

static test_sfp_t *
test_sfp_new(sfp_auth_t auth, sfp_variant_t variant)
{
   test_sfp_t *t = bson_malloc0(sizeof(test_sfp_t));

   char *sfp_atlas_uri = _getenv_required("SFP_ATLAS_URI");
   char *sfp_atlas_user = _getenv_required("SFP_ATLAS_USER");
   char *sfp_atlas_password = _getenv_required("SFP_ATLAS_PASSWORD");
   char *sfp_atlas_x509_uri = _getenv_required("SFP_ATLAS_X509_URI");
   char *sfp_atlas_x509_cert = _getenv_required("SFP_ATLAS_X509_CERT");

   mongoc_uri_t *uri = NULL;
   switch (auth) {
   case SFP_AUTH_UNAUTHENTICATED:
      uri = mongoc_uri_new(sfp_atlas_uri);
      BSON_ASSERT(uri);
      break;
   case SFP_AUTH_SCRAM:
      uri = mongoc_uri_new(sfp_atlas_uri);
      BSON_ASSERT(uri);
      mongoc_uri_set_username(uri, sfp_atlas_user);
      mongoc_uri_set_password(uri, sfp_atlas_password);
      break;
   case SFP_AUTH_X509:
      uri = mongoc_uri_new(sfp_atlas_x509_uri);
      BSON_ASSERT(uri);
      mongoc_uri_set_option_as_utf8(uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, sfp_atlas_x509_cert);
      break;
   default:
      abort();
   }

   // Setting serverSelectionTryOnce=false is to make tests more resilient to transient errors and more consistent with
   // other non-single-threaded Drivers which implicitly set this by default.
   mongoc_uri_set_option_as_bool(uri, MONGOC_URI_SERVERSELECTIONTRYONCE, false);
   if (variant == SFP_VARIANT_COMPRESSED) {
      mongoc_uri_set_option_as_utf8(uri, MONGOC_URI_COMPRESSORS, "zlib,snappy,zstd");
   }

   t->client = mongoc_client_new_from_uri(uri);

   if (variant == SFP_VARIANT_SERVER_API) {
      mongoc_server_api_t *server_api = mongoc_server_api_new(MONGOC_SERVER_API_V1);
      bson_error_t error;
      bool ok = mongoc_client_set_server_api(t->client, server_api, &error);
      if (!ok) {
         fprintf(stderr, "mongoc_client_set_server_api failed: %s\n", error.message);
         abort();
      }
      mongoc_server_api_destroy(server_api);
   }

   // Drivers MUST use a unique collection name for each test run.
   bson_oid_t oid;
   bson_oid_init(&oid, NULL);
   char oid_str[25];
   bson_oid_to_string(&oid, oid_str);
   char *coll_name = bson_strdup_printf("sfp_test_%s", oid_str);
   t->coll = mongoc_client_get_collection(t->client, "db", coll_name);
   bson_free(coll_name);

   mongoc_uri_destroy(uri);

   bson_free(sfp_atlas_x509_cert);
   bson_free(sfp_atlas_x509_uri);
   bson_free(sfp_atlas_password);
   bson_free(sfp_atlas_user);
   bson_free(sfp_atlas_uri);
   return t;
}

static void
test_sfp_drop_collection(test_sfp_t *t)
{
   mongoc_collection_drop(t->coll, NULL);
}

static void
test_sfp_destroy(test_sfp_t *t)
{
   mongoc_collection_destroy(t->coll);
   mongoc_client_destroy(t->client);
   bson_free(t);
}

static bool
test_ping(test_sfp_t *t)
{
   bson_error_t error;
   bson_t *cmd = BCON_NEW("ping", BCON_INT32(1));
   bool ok = mongoc_client_command_simple(t->client, "admin", cmd, NULL, NULL, &error);
   bson_destroy(cmd);
   if (!ok) {
      fprintf(stderr, "ping failed: %s\n", error.message);
   }
   return ok;
}

static bool
test_connection_status(test_sfp_t *t, bool authenticated)
{
   bool ret = false;
   bson_error_t error;
   bson_t reply = BSON_INITIALIZER;
   bson_t *cmd = BCON_NEW("connectionStatus", BCON_INT32(1));
   if (!mongoc_client_command_simple(t->client, "admin", cmd, NULL, &reply, &error)) {
      fprintf(stderr, "connectionStatus failed: %s\n", error.message);
      bson_destroy(cmd);
      goto fail;
   }
   bson_destroy(cmd);

   bson_iter_t iter;
   if (!bson_iter_init_find(&iter, &reply, "authInfo") || !bson_iter_recurse(&iter, &iter) ||
       !bson_iter_find(&iter, "authenticatedUsers") || !bson_iter_recurse(&iter, &iter)) {
      fprintf(stderr, "failed to find authInfo.authenticatedUsers\n");
      goto fail;
   }

   bool has_any = bson_iter_next(&iter);
   if (authenticated && !has_any) {
      fprintf(stderr, "expected at least one authenticated user, but found none\n");
      goto fail;
   } else if (!authenticated && has_any) {
      fprintf(stderr, "expected no authenticated users, but found at least one\n");
      goto fail;
   }

   ret = true;
fail:
   bson_destroy(&reply);
   return ret;
}

static bool
test_crud_operations(test_sfp_t *t)
{
   bool ret = false;
   bson_error_t error;
   mongoc_cursor_t *cursor = NULL;
   bson_t *expected = NULL;

   bson_t *doc_to_insert = BCON_NEW("_id", BCON_INT32(0));
   bool ok = mongoc_collection_insert_one(t->coll, doc_to_insert, NULL, NULL, &error);
   bson_destroy(doc_to_insert);
   if (!ok) {
      fprintf(stderr, "insert failed: %s\n", error.message);
      goto fail;
   }

   bson_t filter = BSON_INITIALIZER;
   cursor = mongoc_collection_find_with_opts(t->coll, &filter, NULL, NULL);

   const bson_t *doc;
   if (!mongoc_cursor_next(cursor, &doc)) {
      if (mongoc_cursor_error(cursor, &error)) {
         fprintf(stderr, "find failed: %s\n", error.message);
      } else {
         fprintf(stderr, "find got no results\n");
      }
      goto fail;
   }

   expected = BCON_NEW("_id", BCON_INT32(0));
   if (!bson_equal(doc, expected)) {
      char *got = bson_as_relaxed_extended_json(doc, NULL);
      fprintf(stderr, "find got unexpected document: %s\n", got);
      bson_free(got);
      goto fail;
   }

   ret = true;
fail:
   bson_destroy(expected);
   mongoc_cursor_destroy(cursor);
   return ret;
}

static void
run_test(sfp_auth_t auth_type, sfp_variant_t variant)
{
   test_sfp_t *t = test_sfp_new(auth_type, variant);
   bool ok = test_ping(t);
   ok = test_connection_status(t, auth_type != SFP_AUTH_UNAUTHENTICATED) && ok;
   if (auth_type != SFP_AUTH_UNAUTHENTICATED) {
      ok = test_crud_operations(t) && ok;
   }
   test_sfp_drop_collection(t); // Drop before asserting.
   if (!ok) {
      fprintf(
         stderr, "%s test FAILED for variant: %s\n", sfp_auth_to_string(auth_type), sfp_variant_to_string(variant));
      abort();
   }

   test_sfp_destroy(t);
}

int
main(void)
{
   mongoc_init();

   run_test(SFP_AUTH_UNAUTHENTICATED, SFP_VARIANT_BASELINE);
   run_test(SFP_AUTH_SCRAM, SFP_VARIANT_BASELINE);
   run_test(SFP_AUTH_SCRAM, SFP_VARIANT_COMPRESSED);
   run_test(SFP_AUTH_SCRAM, SFP_VARIANT_SERVER_API);
   run_test(SFP_AUTH_X509, SFP_VARIANT_BASELINE);
   run_test(SFP_AUTH_X509, SFP_VARIANT_COMPRESSED);
   run_test(SFP_AUTH_X509, SFP_VARIANT_SERVER_API);

   mongoc_cleanup();
   return 0;
}

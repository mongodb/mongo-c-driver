#include <stdio.h>

#include <mongoc/mongoc.h>

#include "common-thread-private.h"

static BSON_THREAD_FUN (_run_ping, data)
{
   mongoc_client_t *client = data;
   mongoc_database_t *db = NULL;
   bson_error_t error;
   bson_t *ping = BCON_NEW ("ping", BCON_INT32 (1));
   bool ok = true;

   db = mongoc_client_get_database (client, "testdb");
   if (!db) {
      fprintf (stderr, "Failed to get DB\n");
      exit (EXIT_FAILURE);
   }
   ok = mongoc_database_command_with_opts (db, ping, NULL /* read_prefs */, NULL /* opts */, NULL /* reply */, &error);
   bson_destroy (ping);
   if (!ok) {
      fprintf (stderr, "ERROR MSG: %s\n", error.message);
      exit (EXIT_FAILURE);
   }
   mongoc_database_destroy (db);

   BSON_THREAD_RETURN;
}

/* Remove all characters after a whitespce character */
static void
_truncate_on_whitespace (char *str)
{
   for (size_t i = 0; str[i] != '\0'; i++) {
      if (isspace (str[i])) {
         str[i] = '\0';
         break;
      }
   }
}

static bool
_oidc_callback (const mongoc_oidc_callback_params_t *params, mongoc_oidc_credential_t *cred /* OUT */)
{
   int64_t timeout = 0;
   int64_t version = 0;
   long size = 0;
   FILE *token_file = NULL;
   int rc = 0;
   char *token = NULL;
   bool ok = true;
   size_t nread = 0;

   token_file = fopen ("/tmp/tokens/test_user1", "r");
   if (!token_file) {
      perror ("fopen");
      ok = false;
      goto done;
   }

   /* Get size of token file */
   rc = fseek (token_file, 0, SEEK_END);
   if (rc != 0) {
      perror ("seek");
      ok = false;
      goto done;
   }
   size = ftell (token_file);
   if (size < 0) {
      perror ("ftell");
      ok = false;
      goto done;
   }
   rewind (token_file);

   /* Allocate buffer for token string */
   token = malloc (size + 1);
   if (!token) {
      ok = false;
      goto done;
   }

   /* Read file into token buffer */
   nread = fread (token, 1, size, token_file);
   if (nread != size) {
      perror ("fread");
      ok = false;
      goto done;
   }
   token[size] = '\0';

   /* The file might have trailing whitespaces such as "\n" or "\r\n"*/
   _truncate_on_whitespace (token);

   timeout = mongoc_oidc_callback_params_get_timeout_ms (params);
   version = mongoc_oidc_callback_params_get_version (params);

   /* Provide your OIDC token to the MongoDB C Driver via the 'creds' out
    * parameter. Remember to free your token string. The C driver stores its
    * own copy */
   mongoc_oidc_credential_set_access_token (cred, token);
   mongoc_oidc_credential_set_expires_in_seconds (cred, 200);

   printf ("version: %" PRId64 "\n", version);
   printf ("timeout: %" PRId64 "\n", timeout);

done:
   if (token_file) {
      fclose (token_file);
   }
   free (token);
   return ok;
}

bool
connect_with_oidc (void)
{
   const char *uri_str = "mongodb://admin@localhost/?authMechanism=MONGODB-OIDC";
   mongoc_client_t *client = NULL;
   bson_error_t error = {0};
   mongoc_uri_t *uri = NULL;
   bool ok = true;

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      fprintf (stderr, "Failed to create URI: '%s': %s\n", uri_str, error.message);
      ok = false;
      goto done;
   }

   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      fprintf (stderr, "Failed to get client\n");
      ok = false;
      goto done;
   }

   mongoc_client_set_oidc_callback (client, _oidc_callback);

   _run_ping (client);

   fprintf (stderr, "Authentication was successful!\n");

done:
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   return ok;
}

enum {
   NUM_THREADS = 100,
};

bool
connect_with_oidc_pooled (void)
{
   const char *uri_str = "mongodb://localhost/?authMechanism=MONGODB-OIDC";
   mongoc_client_pool_t *pool = NULL;
   bson_error_t error = {0};
   mongoc_uri_t *uri = NULL;
   bson_thread_t threads[NUM_THREADS];
   bool ok = true;
   mongoc_client_t *clients[NUM_THREADS];

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      fprintf (stderr, "Failed to create URI: '%s': %s\n", uri_str, error.message);
      ok = false;
      goto done;
   }

   pool = mongoc_client_pool_new (uri);
   if (!pool) {
      fprintf (stderr, "Failed to get client\n");
      ok = false;
      goto done;
   }

   mongoc_client_pool_set_oidc_callback (pool, _oidc_callback);

   for (size_t i = 0; i < NUM_THREADS; i++) {
      clients[i] = mongoc_client_pool_pop (pool);
      mcommon_thread_create (&threads[i], _run_ping, clients[i]);
   }

   for (size_t i = 0; i < NUM_THREADS; i++) {
      mcommon_thread_join (threads[i]);
      mongoc_client_pool_push (pool, clients[i]);
   }

   fprintf (stderr, "Authentication was successful!\n");

done:
   mongoc_uri_destroy (uri);
   mongoc_client_pool_destroy (pool);
   return ok;
}

int
main (void)
{
   mongoc_init ();

   int rc = 0;
   bool ok = true;

   ok = connect_with_oidc ();
   if (!ok) {
      fprintf (stderr, "Authentication failed\n");
      rc = 1;
      goto done;
   }

   ok = connect_with_oidc_pooled ();
   if (!ok) {
      fprintf (stderr, "Authentication failed\n");
      rc = 1;
      goto done;
   }

done:
   mongoc_cleanup ();
   return rc;
}

/*
 * OIDC Prose tests, see specification here:
 * https://github.com/mongodb/specifications/blob/474ddfcc335225df4410986be2b10ae41a736d20/source/auth/tests/mongodb-oidc.rst#1callback-driven-auth
 */

#include <stdio.h>

#include <mongoc/mongoc.h>
#include <mongoc/mongoc-thread-private.h>

static char *username = "test_user1";
static bson_mutex_t _callback_counter_mtx;
static size_t callback_counter = 0;

enum { NUM_THREADS = 10 };

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
   char *token_file_path = NULL;

   bson_mutex_lock (&_callback_counter_mtx);
   callback_counter++;
   bson_mutex_unlock (&_callback_counter_mtx);

   token_file_path = bson_strdup_printf ("/tmp/tokens/%s", username);

   token_file = fopen (token_file_path, "r");
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
   token = bson_malloc (size + 1);
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

   /* The file might have trailing whitespaces such as "\n" or "\r\n" */
   _truncate_on_whitespace (token);

   version = mongoc_oidc_callback_params_get_version (params);
   timeout = mongoc_oidc_callback_params_get_timeout_ms (params);

   BSON_ASSERT (version == 1);
   BSON_ASSERT (timeout == 60000);

   /* Provide your OIDC token to the MongoDB C Driver via the 'creds' out
    * parameter. Remember to free your token string. The C driver stores its
    * own copy */
   mongoc_oidc_credential_set_access_token (cred, token);
   mongoc_oidc_credential_set_expires_in_seconds (cred, 200);

done:
   if (token_file) {
      fclose (token_file);
   }
   bson_free (token_file_path);
   bson_free (token);
   return ok;
}

static bool
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

done:
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   return ok;
}

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
      fprintf (stderr, "Failed to get pool\n");
      ok = false;
      goto done;
   }

   mongoc_client_pool_set_oidc_callback (pool, _oidc_callback);

   for (size_t i = 0; i < NUM_THREADS; i++) {
      clients[i] = mongoc_client_pool_pop (pool);
      BSON_ASSERT (clients[i]);
      mcommon_thread_create (&threads[i], _run_ping, clients[i]);
   }

   for (size_t i = 0; i < NUM_THREADS; i++) {
      mcommon_thread_join (threads[i]);
      mongoc_client_pool_push (pool, clients[i]);
   }

done:
   mongoc_uri_destroy (uri);
   return ok;
}

static bool
ping_server (void)
{
   bool ok = true;

   ok = connect_with_oidc ();
   if (!ok) {
      fprintf (stderr, "single threaded OIDC authentication failed\n");
      goto done;
   }

   ok = connect_with_oidc_pooled ();
   if (!ok) {
      fprintf (stderr, "pooled OIDC authentication failed\n");
      goto done;
   }

done:
   return ok;
}

/* (1) Callback Authentication */

/*
 * 1.1 Callback is called during authentication
 * - Create an OIDC configured client.
 * - Perform a find operation that succeeds.
 * - Assert that the callback was called 1 time.
 * - Close the client.
 */
static bool
callback_is_called_during_authentication (void)
{
   mongoc_client_t *client = NULL;
   mongoc_collection_t *coll = NULL;
   bson_t *query = NULL;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   const bson_t *reply;
   const char *uri_str = "mongodb://localhost:27017/?authMechanism=MONGODB-OIDC";
   mongoc_uri_t *uri = NULL;
   bson_error_t error;
   bool ok = true;

   callback_counter = 0;

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      fprintf (stderr, "Failed to create URI: '%s': %s\n", uri_str, error.message);
      ok = false;
      goto done;
   }

   client = mongoc_client_new_from_uri (uri);
   mongoc_client_set_oidc_callback (client, _oidc_callback);

   coll = mongoc_client_get_collection (client, "test", "test");
   query = bson_new ();
   cursor = mongoc_collection_find_with_opts (coll, query, NULL, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
      ;
   }

   ok = !mongoc_cursor_error_document (cursor, &error, &reply);
   if (!ok) {
      char *json = bson_as_json (reply, NULL);
      fprintf (stderr, "Cursor Failure: %s\nReply: %s\n", error.message, json);
      bson_free (json);
      goto done;
   }

   BSON_ASSERT (callback_counter == 1);

done:
   mongoc_cursor_destroy (cursor);
   bson_destroy (query);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);

   return ok;
}

static void *
multiple_connections_thread_func (void *data)
{
   mongoc_collection_t *coll = NULL;
   mongoc_cursor_t *cursor = NULL;
   bson_t *query = NULL;
   const bson_t *reply;
   bson_error_t error;
   const bson_t *doc;
   bool ok;

   mongoc_client_t *client = data;

   for (size_t i = 0; i < 100; i++) {
      coll = mongoc_client_get_collection (client, "test", "test");
      query = bson_new ();
      cursor = mongoc_collection_find_with_opts (coll, query, NULL, NULL);

      while (mongoc_cursor_next (cursor, &doc)) {
         ;
      }

      ok = !mongoc_cursor_error_document (cursor, &error, &reply);
      if (!ok) {
         char *json = bson_as_json (reply, NULL);
         fprintf (stderr, "Cursor Failure: %s\nReply: %s\n", error.message, json);
         bson_free (json);
         BSON_ASSERT (ok);
      }

      mongoc_cursor_destroy (cursor);
      bson_destroy (query);
      mongoc_collection_destroy (coll);
   }

   return NULL;
}

/*
 * 1.2 Callback is called once for multiple connections
 * - Create an OIDC configured client.
 * - Start 10 threads and run 100 find operations in each thread that all succeed.
 * - Assert that the callback was called 1 time.
 * - Close the client.
 */
static bool
callback_is_called_once_for_multiple_connections (void)
{
   mongoc_client_pool_t *pool = NULL;
   bson_error_t error;
   const char *uri_str = "mongodb://localhost:27017/?authMechanism=MONGODB-OIDC";
   mongoc_uri_t *uri = NULL;
   bool ok = true;
   pthread_t threads[NUM_THREADS] = {0};
   mongoc_client_t *clients[NUM_THREADS];

   callback_counter = 0;

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      fprintf (stderr, "Failed to create URI: '%s': %s\n", uri_str, error.message);
      ok = false;
      goto done;
   }

   pool = mongoc_client_pool_new (uri);
   mongoc_client_pool_set_oidc_callback (pool, _oidc_callback);

   for (size_t i = 0; i < NUM_THREADS; i++) {
      clients[i] = mongoc_client_pool_pop (pool);
      BSON_ASSERT (clients[i]);
      mcommon_thread_create (&threads[i], multiple_connections_thread_func, clients[i]);
   }

   for (size_t i = 0; i < NUM_THREADS; i++) {
      mcommon_thread_join (threads[i]);
      mongoc_client_pool_push (pool, clients[i]);
   }

   BSON_ASSERT (callback_counter == 1);

done:
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);

   return ok;
}

/* (2) OIDC Callback Validation */

/*
 * 2.1 Valid Callback Inputs
 * - Create an OIDC configured client with an OIDC callback that validates its inputs and returns a valid access token.
 * - Perform a find operation that succeeds.
 * - Assert that the OIDC callback was called with the appropriate inputs, including the timeout parameter if possible.
 * - Close the client.
 */
static bool
valid_callback_inputs (void)
{
   mongoc_client_t *client = NULL;
   mongoc_collection_t *coll = NULL;
   bson_t *query = NULL;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   const bson_t *reply;
   const char *uri_str = "mongodb://localhost:27017/?authMechanism=MONGODB-OIDC";
   mongoc_uri_t *uri = NULL;
   bson_error_t error;
   bool ok = true;

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      fprintf (stderr, "Failed to create URI: '%s': %s\n", uri_str, error.message);
      ok = false;
      goto done;
   }

   client = mongoc_client_new_from_uri (uri);
   mongoc_client_set_oidc_callback (client, _oidc_callback);

   coll = mongoc_client_get_collection (client, "test", "test");
   query = bson_new ();
   cursor = mongoc_collection_find_with_opts (coll, query, NULL, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
      ;
   }

   ok = !mongoc_cursor_error_document (cursor, &error, &reply);
   if (!ok) {
      char *json = bson_as_json (reply, NULL);
      fprintf (stderr, "Cursor Failure: %s\nReply: %s\n", error.message, json);
      bson_free (json);
      goto done;
   }

done:
   mongoc_cursor_destroy (cursor);
   bson_destroy (query);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);

   return ok;
}

static bool
_oidc_failing_callback (const mongoc_oidc_callback_params_t *params, mongoc_oidc_credential_t *cred /* OUT */)
{
   BSON_ASSERT (params);
   BSON_ASSERT (cred);

   mongoc_oidc_credential_set_access_token (cred, NULL);
   return false;
}

/*
 * 2.2 OIDC Callback Returns Null
 *
 * - Create an OIDC configured client with an OIDC callback that returns null.
 * - Perform a find operation that fails.
 * - Close the client.
 */
static bool
oidc_callback_returns_null (void)
{
   mongoc_client_t *client = NULL;
   mongoc_collection_t *coll = NULL;
   bson_t *query = NULL;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   const bson_t *reply;
   const char *uri_str = "mongodb://localhost:27017/?authMechanism=MONGODB-OIDC";
   mongoc_uri_t *uri = NULL;
   bson_error_t error;
   bool ok = true;

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      fprintf (stderr, "Failed to create URI: '%s': %s\n", uri_str, error.message);
      ok = false;
      goto done;
   }

   client = mongoc_client_new_from_uri (uri);

   /* Set the callback to a callback that errors and sets the token to NULL. */
   mongoc_client_set_oidc_callback (client, _oidc_failing_callback);

   coll = mongoc_client_get_collection (client, "test", "test");
   query = bson_new ();
   cursor = mongoc_collection_find_with_opts (coll, query, NULL, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
      ;
   }

   /* Assert that the 'find' operation fails. */
   BSON_ASSERT (!mongoc_cursor_error_document (cursor, &error, &reply));

done:
   mongoc_cursor_destroy (cursor);
   bson_destroy (query);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);

   return ok;
}

int
main (void)
{
   mongoc_init ();
   bson_mutex_init (&_callback_counter_mtx);

   BSON_ASSERT (callback_is_called_during_authentication ());
   BSON_ASSERT (callback_is_called_once_for_multiple_connections ());
   BSON_ASSERT (valid_callback_inputs ());
   BSON_ASSERT (oidc_callback_returns_null ());

done:
   bson_mutex_destroy (&_callback_counter_mtx);
   mongoc_cleanup ();
   return 0;
}

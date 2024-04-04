#include <stdio.h>

#include <mongoc/mongoc.h>

static bool
_run_ping (mongoc_database_t *db)
{
   bson_error_t error;
   bson_t *ping = BCON_NEW ("ping", BCON_INT32 (1));
   bool ok = mongoc_database_command_with_opts (db, ping, NULL /* read_prefs */, NULL /* opts */, NULL /* reply */, &error);
   bson_destroy (ping);
   if (!ok) {
      fprintf(stderr, "ERROR MSG: %s\n", error.message);
   }
   return ok;
}

/* Remove all characters after a whitespce character */
static void
_truncate_on_whitespace(char *str)
{
   for (size_t i = 0; str[i] != '\0'; i++) {
      if (isspace(str[i])) {
         str[i] = '\0';
         break;
      }
   }
}

static bool
_oidc_callback(const mongoc_oidc_callback_params_t *params, mongoc_oidc_credential_t *cred /* OUT */) {
   int64_t timeout = 0;
   int64_t version = 0;
   long size = 0;
   FILE *token_file = NULL;
   int rc = 0;
   char *token = NULL;
   bool ok = true;
   size_t nread = 0;

   token_file = fopen("/tmp/tokens/test_user1", "r");
   if (!token_file) {
      perror("fopen");
      ok = false;
      goto done;
   }

   /* Get size of token file */
   rc = fseek(token_file, 0, SEEK_END);
   if (rc != 0) {
      perror("seek");
      ok = false;
      goto done;
   }
   size = ftell(token_file);
   if (size < 0) {
      perror("ftell");
      ok = false;
      goto done;
   }
   rewind(token_file);

   /* Allocate buffer for token string */
   token = malloc(size + 1);
   if (!token) {
      ok = false;
      goto done;
   }

   /* Read file into token buffer */
   nread = fread(token, 1, size, token_file);
   if (nread != size) {
      perror("fread");
      ok = false;
      goto done;
   }
   token[size] = '\0';

   /* The file might have trailing whitespaces such as "\n" or "\r\n"*/
   _truncate_on_whitespace(token);

   timeout = mongoc_oidc_callback_params_get_timeout_ms(params);
   version = mongoc_oidc_callback_params_get_version(params);

   /* Provide your OIDC token to the MongoDB C Driver via the 'creds' out
    * parameter. Remember to free your token string. The C driver stores its
    * own copy */
   mongoc_oidc_credential_set_access_token(cred, token);
   mongoc_oidc_credential_set_expires_in_seconds(cred, 200);

done:
   if (token_file) {
      fclose(token_file);
   }
   free(token);
   return ok;
}

bool
connect_with_oidc(void)
{
   const char *uri_str = "mongodb://admin@localhost/?authMechanism=MONGODB-OIDC";
   mongoc_database_t *db = NULL;
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

   db = mongoc_client_get_database (client, "testdb");
   if (!db) {
      fprintf (stderr, "Failed to get DB\n");
      ok = false;
      goto done;
   }

   mongoc_client_set_oidc_callback(client,  _oidc_callback);

   ok = _run_ping(db);
   if (!ok) {
      goto done;
   }

   fprintf(stderr, "Authentication was successful!\n");

done:
    return ok;
}

int main(void) {
    int rc = 0;

    bool ok = connect_with_oidc();
    if (!ok) {
       fprintf(stderr, "Authentication failed\n");
       rc = 1;
    }

    return rc;
}

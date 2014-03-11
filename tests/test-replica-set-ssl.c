#include <string.h>

#include "ha-test.h"

#include "mongoc-client.h"
#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-tests.h"
#include "mongoc-write-concern-private.h"

#define TRUST_DIR "tests/trust_dir"
#define CAFILE TRUST_DIR "/verify/mongo_root.pem"
#define PEMFILE_LOCALHOST TRUST_DIR "/keys/127.0.0.1.pem"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "test"

static char * gTestCAFile;
static char * gTestPEMFileLocalhost;

static void
test_replica_set_ssl_client(void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   ha_replica_set_t *replica_set;
   bson_error_t error;
   int r;
   bson_t b;

   mongoc_ssl_opt_t sopt = { 0 };

   sopt.pem_file = gTestPEMFileLocalhost;
   sopt.ca_file = gTestCAFile;

   replica_set = ha_replica_set_new("repltest1");
   ha_replica_set_ssl(replica_set, &sopt);
   ha_replica_set_add_replica(replica_set, "replica1");
   ha_replica_set_add_replica(replica_set, "replica2");
   ha_replica_set_add_replica(replica_set, "replica3");

   ha_replica_set_start(replica_set);
   ha_replica_set_wait_for_healthy(replica_set);


   client = ha_replica_set_create_client(replica_set);
   assert(client);

   collection = mongoc_client_get_collection(client, "test", "test");
   assert(collection);

   bson_init(&b);
   bson_append_utf8(&b, "hello", -1, "world", -1);

   r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL, &error);
   assert(r);

   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
   bson_destroy(&b);

   ha_replica_set_shutdown(replica_set);
   ha_replica_set_destroy(replica_set);
}


static void
log_handler (mongoc_log_level_t log_level,
             const char        *domain,
             const char        *message,
             void              *user_data)
{
   /* Do Nothing */
}


int
main (int   argc,   /* IN */
      char *argv[]) /* IN */
{
   char *cwd;
   char buf[1024];

   if (argc <= 1 || !!strcmp (argv[1], "-v")) {
      mongoc_log_set_handler (log_handler, NULL);
   }

   mongoc_init ();

   cwd = getcwd(buf, sizeof(buf));
   assert(cwd);

   gTestCAFile = bson_strdup_printf("%s/" CAFILE, cwd);
   gTestPEMFileLocalhost = bson_strdup_printf("%s/" PEMFILE_LOCALHOST, cwd);

   run_test("/ReplicaSet/ssl/client", &test_replica_set_ssl_client);

   bson_free(gTestCAFile);
   bson_free(gTestPEMFileLocalhost);

   mongoc_cleanup();

   return 0;
}

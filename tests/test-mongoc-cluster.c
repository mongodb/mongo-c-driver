#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mock-server.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cluster-test"


void
call_ismaster (bson_t *reply)
{
   bson_t ismaster = BSON_INITIALIZER;
   mongoc_client_t *client;
   bool r;

   BSON_APPEND_INT32 (&ismaster, "isMaster", 1);
   client = test_framework_client_new (NULL);
   r = mongoc_client_command_simple (client, "admin", &ismaster,
                                     NULL, reply, NULL);

   assert (r);
}


char *
set_name (bson_t *ismaster_response)
{
   bson_iter_t iter;

   if (bson_iter_init_find (&iter, ismaster_response, "setName")) {
      return bson_strdup (bson_iter_utf8 (&iter, NULL));
   } else {
      return NULL;
   }
}


int
get_n_members (bson_t *ismaster_response)
{
   bson_iter_t iter;
   bson_iter_t hosts_iter;
   char *name;
   int n;

   if ((name = set_name (ismaster_response))) {
      bson_free (name);
      assert (bson_iter_init_find (&iter, ismaster_response, "hosts"));
      bson_iter_recurse (&iter, &hosts_iter);
      n = 0;
      while (bson_iter_next (&hosts_iter)) n++;
      return n;
   } else {
      return 1;
   }
}


#define BAD_HOST "mongodb.com:12345"


/* a uri with one bogus host */
mongoc_uri_t *
uri_from_ismaster_plus_one (bson_t *ismaster_response)
{
   /* start with one bad host and a comma */
   bson_string_t *uri_str = bson_string_new ("mongodb://" BAD_HOST ",");
   char *name;
   bson_iter_t iter;
   bson_iter_t hosts_iter;

   if ((name = set_name (ismaster_response))) {
      bson_iter_init_find (&iter, ismaster_response, "hosts");
      bson_iter_recurse (&iter, &hosts_iter);
      while (bson_iter_next (&hosts_iter)) {
         assert (BSON_ITER_HOLDS_UTF8 (&hosts_iter));
         bson_string_append (uri_str, bson_iter_utf8 (&hosts_iter, NULL));
         while (bson_iter_next (&hosts_iter)) {
            bson_string_append (uri_str, ",");
            bson_string_append (uri_str, bson_iter_utf8 (&hosts_iter, NULL));
         }

         bson_string_append_printf (
            uri_str, "/?replicaSet=%s&connecttimeoutms=1000", name);
      }

      bson_free (name);
   } else {
      char *host = test_framework_get_host ();

      bson_string_append (uri_str, host);
      bson_string_append (uri_str, "/?connecttimeoutms=1000");

      bson_free (host);
   }

   return mongoc_uri_new (bson_string_free (uri_str, false));
}


bool
cluster_has_host (const mongoc_cluster_t *cluster,
                  const char *host_and_port)
{
   int i;

   for (i = 0; i < cluster->nodes_len; i++) {
      if (!strcmp(cluster->nodes[i].host.host_and_port, host_and_port)) {
         return true;
      }
   }

   return false;
}


int
hosts_len (const mongoc_host_list_t *hl)
{
   int n = 0;

   if (!hl) {
      return 0;
   }

   do { n++; } while ((hl = hl->next));

   return n;
}


void
assert_hosts_equal (const mongoc_host_list_t *hl,
                    const mongoc_cluster_t *cluster)
{
   ASSERT_CMPINT (hosts_len (hl), ==, cluster->nodes_len);

   while (hl) {
      if (!cluster_has_host (cluster, hl->host_and_port) &&
          strcmp (BAD_HOST, hl->host_and_port)) {
         printf ("cluster has no host %s\n", hl->host_and_port);
         abort ();
      }

      hl = hl->next;
   }
}


/* not very exhaustive, but ensure that cluster reflects whatever server
 * we're connected to */
static void
test_mongoc_cluster_basic (void)
{
   mongoc_client_t *client;
   bson_t reply;
   mongoc_uri_t *uri;
   const mongoc_host_list_t *hosts;
   bson_error_t error;
   int n_members;
   char *replica_set_name;
   int i;
   int n;

   call_ismaster (&reply);
   n_members = get_n_members (&reply);
   replica_set_name = set_name (&reply);
   uri = uri_from_ismaster_plus_one (&reply);

   /* get hosts list from uri */
   hosts = mongoc_uri_get_hosts (uri);
   assert (hosts);
   ASSERT_CMPSTR (BAD_HOST, hosts->host_and_port);

   if (replica_set_name) {
      /* remove bad host we prepended, because the cluster will remove it
       * once it finds the primary */
      hosts = hosts->next;
      assert (hosts);
   }

   client = mongoc_client_new_from_uri (uri);

   ASSERT_CMPINT (n_members, ==, client->cluster.nodes_len - 1);
   if (replica_set_name) {
      ASSERT_CMPINT (MONGOC_CLUSTER_REPLICA_SET, ==, client->cluster.mode);
   } else {
      /* sharded mode, since we gave 2 seeds */
      ASSERT_CMPINT (MONGOC_CLUSTER_SHARDED_CLUSTER, ==, client->cluster.mode);
   }

   /* connect twice and assert cluster nodes are as expected */
   for (i = 0; i < 2; i++) {
      /* warnings about failing to connect to mongodb.com:12345 */
      suppress_one_message ();
      suppress_one_message ();
      suppress_one_message ();

      _mongoc_cluster_reconnect (&client->cluster, &error);

      assert_hosts_equal (hosts, &client->cluster);

      for (n = 0; n < client->cluster.nodes_len; n++) {
         const mongoc_cluster_node_t *node = &client->cluster.nodes[n];
         bool valid_host;

         assert (node->valid);

         valid_host = (strlen (node->host.host_and_port) &&
                       strcmp (node->host.host_and_port, BAD_HOST));

         if (valid_host) {
            assert (node->stream);
         } else {
            assert (!node->stream);
         }

         ASSERT_CMPINT (n, ==, node->index);
         ASSERT_CMPINT (0, ==, node->stamp);
         ASSERT_CMPSTR (replica_set_name, node->replSet);
      }
   }

   bson_free (replica_set_name);
   mongoc_uri_destroy (uri);
   bson_destroy (&reply);
   mongoc_client_destroy (client);
}


static void
_test_mongoc_cluster_destroy_disconnect (bool has_many_tags,
                                         bool rs_connection)
{
   uint16_t port;
   char *uri_str;
   mongoc_uri_t *uri;
   const mongoc_host_list_t *hosts;
   int i;
   bson_t tags = BSON_INITIALIZER;
   char *key;
   mock_server_t *server;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_cluster_t *cluster;
   mongoc_cluster_node_t *node = NULL;

   port = 20000 + (rand () % 1000);
   uri_str = bson_strdup_printf (
      rs_connection ? "mongodb://localhost:%hu/?replicaSet=rs"
                    : "mongodb://localhost:%hu/",
      port);

   uri = mongoc_uri_new (uri_str);
   hosts = mongoc_uri_get_hosts (uri);

   if (has_many_tags) {
      for (i = 0; i < 100; i++) {
         /* ensure the tags document must spill to the heap */
         key = bson_strdup_printf ("key%d", i);
         BSON_APPEND_UTF8 (
            &tags, key,
            "value-value-value-value-value-value-value-value-value-value");

         bson_free (key);
      }
   } else {
      BSON_APPEND_UTF8 (&tags, "key", "value");
   }

   server = mock_server_new_rs ("127.0.0.1", port, NULL, NULL,
                                "rs", true, false, hosts, &tags);

   mock_server_run_in_thread (server);

   client = mongoc_client_new (uri_str);
   cluster = &client->cluster;

   for (i = 0; i < 2; i++) {
      assert (_mongoc_cluster_reconnect (cluster, &error));
      ASSERT_CMPINT (cluster->nodes_len, ==, 1);
      node = &cluster->nodes[0];
      if (rs_connection) {
         ASSERT_CMPSTR (node->replSet, "rs");
         if (has_many_tags) {
            ASSERT_CMPINT (100, ==, bson_count_keys (&node->tags));
         } else {
            ASSERT_CMPINT (1, ==, bson_count_keys (&node->tags));
         }
      } else {
         assert (!node->replSet);
         /* cluster ignores "ismaster.tags" in direct mode */
         assert (bson_empty (&node->tags));
      }
   }

   mock_server_quit (server);
   mock_server_destroy (server);

   /* no segfaults */
   _mongoc_cluster_node_destroy (node);
   _mongoc_cluster_disconnect_node (cluster, node);
   mongoc_client_destroy (client);

   bson_destroy (&tags);
   mongoc_uri_destroy (uri);
   bson_free (uri_str);
}


void
test_mongoc_cluster_destroy_disconnect_one_direct (void)
{
   _test_mongoc_cluster_destroy_disconnect (false, false);
}


void
test_mongoc_cluster_destroy_disconnect_many_direct (void)
{
   _test_mongoc_cluster_destroy_disconnect (true, false);
}


void
test_mongoc_cluster_destroy_disconnect_one_rs (void)
{
   _test_mongoc_cluster_destroy_disconnect (false, true);
}


void
test_mongoc_cluster_destroy_disconnect_many_rs (void)
{
   _test_mongoc_cluster_destroy_disconnect (true, true);
}


void
test_cluster_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Cluster/basic", test_mongoc_cluster_basic);
   TestSuite_Add (suite, "/Cluster/node_destroy_disconnect/one_tag/direct",
                  test_mongoc_cluster_destroy_disconnect_one_direct);
   TestSuite_Add (suite, "/Cluster/node_destroy_disconnect/many_tags/direct",
                  test_mongoc_cluster_destroy_disconnect_many_direct);
   TestSuite_Add (suite, "/Cluster/node_destroy_disconnect/one_tag/rs",
                  test_mongoc_cluster_destroy_disconnect_one_rs);
   TestSuite_Add (suite, "/Cluster/node_destroy_disconnect/many_tags/rs",
                  test_mongoc_cluster_destroy_disconnect_many_rs);
}

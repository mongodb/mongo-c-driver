#include <mongoc.h>
#include <netdb.h>

#include "mongoc-tests.h"


static void
test_mongoc_uri_new (void)
{
   const mongoc_host_list_t *hosts;
   const bson_t *options;
   mongoc_uri_t *uri;
   bson_iter_t iter;
   bson_iter_t child;

   assert(!mongoc_uri_new("mongodb://"));
   assert(!mongoc_uri_new("mongodb://::"));
   assert(!mongoc_uri_new("mongodb://localhost::27017"));
   assert(!mongoc_uri_new("mongodb://localhost,localhost::"));
   assert(!mongoc_uri_new("mongodb://local1,local2,local3/d?k"));
   assert(!mongoc_uri_new(""));
   assert(!mongoc_uri_new("mongo://localhost:27017"));
   assert(!mongoc_uri_new("mongodb://localhost::27017"));
   assert(!mongoc_uri_new("mongodb://localhost::27017/"));
   assert(!mongoc_uri_new("mongodb://localhost::27017,abc"));

   /*
    * TODO: Support IPv6.
    */
   assert(!mongoc_uri_new("mongodb://[::1]/?ipv6=true&safe=true"));

   uri = mongoc_uri_new("mongodb:///tmp/mongodb.sock/?");
   assert(uri);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost/?");
   assert(uri);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost:27017/test?q=1");
   assert(uri);
   hosts = mongoc_uri_get_hosts(uri);
   assert(hosts);
   assert(!hosts->next);
   assert_cmpstr(hosts->host, "localhost");
   assert_cmpint(hosts->port, ==, 27017);
   assert_cmpstr(hosts->host_and_port, "localhost:27017");
   assert_cmpstr(mongoc_uri_get_database(uri), "test");
   options = mongoc_uri_get_options(uri);
   assert(options);
   assert(bson_iter_init_find(&iter, options, "q"));
   assert_cmpstr(bson_iter_utf8(&iter, NULL), "1");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://local1,local2:999,local3?q=1");
   assert(uri);
   hosts = mongoc_uri_get_hosts(uri);
   assert(hosts);
   assert(hosts->next);
   assert(hosts->next->next);
   assert(!hosts->next->next->next);
   assert_cmpstr(hosts->host, "local1");
   assert_cmpint(hosts->port, ==, 27017);
   assert_cmpstr(hosts->next->host, "local2");
   assert_cmpint(hosts->next->port, ==, 999);
   assert_cmpstr(hosts->next->next->host, "local3");
   assert_cmpint(hosts->next->next->port, ==, 27017);
   options = mongoc_uri_get_options(uri);
   assert(options);
   assert(bson_iter_init_find(&iter, options, "q"));
   assert_cmpstr(bson_iter_utf8(&iter, NULL), "1");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost:27017/?readPreferenceTags=dc:ny&readPreferenceTags=");
   assert(uri);
   options = mongoc_uri_get_read_preferences(uri);
   assert(options);
   assert_cmpint(bson_count_keys(options), ==, 2);
   assert(bson_iter_init_find(&iter, options, "0"));
   assert(BSON_ITER_HOLDS_DOCUMENT(&iter));
   assert(bson_iter_recurse(&iter, &child));
   assert(bson_iter_next(&child));
   assert_cmpstr(bson_iter_key(&child), "dc");
   assert_cmpstr(bson_iter_utf8(&child, NULL), "ny");
   assert(!bson_iter_next(&child));
   assert(bson_iter_next(&iter));
   assert(BSON_ITER_HOLDS_DOCUMENT(&iter));
   assert(bson_iter_recurse(&iter, &child));
   assert(!bson_iter_next(&child));
   assert(!bson_iter_next(&iter));
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost/a?slaveok=true&ssl=false&journal=true");
   options = mongoc_uri_get_options(uri);
   assert(options);
   assert_cmpint(bson_count_keys(options), ==, 3);
   assert(bson_iter_init(&iter, options));
   assert(bson_iter_find_case(&iter, "slaveok"));
   assert(BSON_ITER_HOLDS_BOOL(&iter));
   assert(bson_iter_bool(&iter));
   assert(bson_iter_find_case(&iter, "ssl"));
   assert(BSON_ITER_HOLDS_BOOL(&iter));
   assert(!bson_iter_bool(&iter));
   assert(bson_iter_find_case(&iter, "journal"));
   assert(BSON_ITER_HOLDS_BOOL(&iter));
   assert(bson_iter_bool(&iter));
   assert(!bson_iter_next(&iter));
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb:///tmp/mongodb.sock/?ssl=false");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_hosts(uri)->host, "/tmp/mongodb.sock");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb:///tmp/mongodb.sock,localhost:27017/?ssl=false");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_hosts(uri)->host, "/tmp/mongodb.sock");
   assert_cmpstr(mongoc_uri_get_hosts(uri)->next->host_and_port, "localhost:27017");
   assert(!mongoc_uri_get_hosts(uri)->next->next);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost:27017,/tmp/mongodb.sock/?ssl=false");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_hosts(uri)->host_and_port, "localhost:27017");
   assert_cmpstr(mongoc_uri_get_hosts(uri)->next->host, "/tmp/mongodb.sock");
   assert(!mongoc_uri_get_hosts(uri)->next->next);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://christian:secret@localhost:27017?authSource=abcd");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_username(uri), "christian");
   assert_cmpstr(mongoc_uri_get_password(uri), "secret");
   assert_cmpstr(mongoc_uri_get_auth_source(uri), "abcd");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://christian:secret@localhost:27017");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_auth_source(uri), "admin");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://christian@localhost:27017");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_username(uri), "christian");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://christian%40realm@localhost:27017");
   assert(uri);
   assert_cmpstr(mongoc_uri_get_username(uri), "christian@realm");
   mongoc_uri_destroy(uri);
}


static void
test_mongoc_host_list_from_string (void)
{
   mongoc_host_list_t host_list = { 0 };

   assert(mongoc_host_list_from_string(&host_list, "localhost:27019"));
   assert(!strcmp(host_list.host_and_port, "localhost:27019"));
   assert(!strcmp(host_list.host, "localhost"));
   assert(host_list.port == 27019);
   assert(host_list.family == AF_INET);
   assert(!host_list.next);
}


static void
test_mongoc_uri_new_for_host_port (void)
{
   mongoc_uri_t *uri;

   uri = mongoc_uri_new_for_host_port("uber", 555);
   assert(uri);
   assert(!strcmp("uber", mongoc_uri_get_hosts(uri)->host));
   assert(!strcmp("uber:555", mongoc_uri_get_hosts(uri)->host_and_port));
   assert(555 == mongoc_uri_get_hosts(uri)->port);
   mongoc_uri_destroy(uri);
}


static void
test_mongoc_uri_unescape (void)
{
#define ASSERT_URIDECODE_STR(_s, _e) \
   do { \
      char *str = mongoc_uri_unescape(_s); \
      assert(!strcmp(str, _e)); \
      bson_free(str); \
   } while (0)
#define ASSERT_URIDECODE_FAIL(_s) \
   do { \
      char *str = mongoc_uri_unescape(_s); \
      assert(!str); \
   } while (0)

   ASSERT_URIDECODE_STR("", "");
   ASSERT_URIDECODE_STR("%40", "@");
   ASSERT_URIDECODE_STR("me%40localhost@localhost", "me@localhost@localhost");
   ASSERT_URIDECODE_STR("%20", " ");
   ASSERT_URIDECODE_STR("%24%21%40%2A%26%5E%21%40%2A%23%26%5E%21%40%23%2A%26"
                        "%5E%21%40%2A%23%26%5E%21%40%2A%26%23%5E%7D%7B%7D%7B"
                        "%22%22%27%7D%7B%5B%5D%3C%3E%3F",
                        "$!@*&^!@*#&^!@#*&^!@*#&^!@*&#^}{}{\"\"'}{[]<>?");

   ASSERT_URIDECODE_FAIL("%");
   ASSERT_URIDECODE_FAIL("%%");
   ASSERT_URIDECODE_FAIL("%%%");
   ASSERT_URIDECODE_FAIL("%FF");
   ASSERT_URIDECODE_FAIL("%CC");
   ASSERT_URIDECODE_FAIL("%00");

#undef ASSERT_URIDECODE_STR
#undef ASSERT_URIDECODE_FAIL
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/uri/new", test_mongoc_uri_new);
   run_test("/mongoc/uri/new_for_host_port", test_mongoc_uri_new_for_host_port);
   run_test("/mongoc/uri/unescape", test_mongoc_uri_unescape);
   run_test("/mongoc/host_list/from_string", test_mongoc_host_list_from_string);

   return 0;
}

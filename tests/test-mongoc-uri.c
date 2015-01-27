#include <mongoc.h>

#include "mongoc-host-list-private.h"

#include "TestSuite.h"


static void
test_mongoc_uri_new (void)
{
   const mongoc_host_list_t *hosts;
   const bson_t *options;
   const bson_t *credentials;
   bson_t properties;
   mongoc_uri_t *uri;
   bson_iter_t iter;
   bson_iter_t child;

   /* bad uris */
   ASSERT(!mongoc_uri_new("mongodb://"));
   ASSERT(!mongoc_uri_new("mongodb://::"));
   ASSERT(!mongoc_uri_new("mongodb://localhost::27017"));
   ASSERT(!mongoc_uri_new("mongodb://localhost,localhost::"));
   ASSERT(!mongoc_uri_new("mongodb://local1,local2,local3/d?k"));
   ASSERT(!mongoc_uri_new(""));
   ASSERT(!mongoc_uri_new("mongo://localhost:27017"));
   ASSERT(!mongoc_uri_new("mongodb://localhost::27017"));
   ASSERT(!mongoc_uri_new("mongodb://localhost::27017/"));
   ASSERT(!mongoc_uri_new("mongodb://localhost::27017,abc"));

   uri = mongoc_uri_new("mongodb://[::1]:27888,[::2]:27999/?ipv6=true&safe=true");
   assert (uri);
   hosts = mongoc_uri_get_hosts(uri);
   assert (hosts);
   ASSERT_CMPSTR (hosts->host, "::1");
   assert (hosts->port == 27888);
   ASSERT_CMPSTR (hosts->host_and_port, "[::1]:27888");
   mongoc_uri_destroy (uri);

   uri = mongoc_uri_new("mongodb:///tmp/mongodb-27017.sock/?");
   ASSERT(uri);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost/?");
   ASSERT(uri);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost:27017/test?q=1");
   ASSERT(uri);
   hosts = mongoc_uri_get_hosts(uri);
   ASSERT(hosts);
   ASSERT(!hosts->next);
   ASSERT_CMPSTR(hosts->host, "localhost");
   ASSERT_CMPINT(hosts->port, ==, 27017);
   ASSERT_CMPSTR(hosts->host_and_port, "localhost:27017");
   ASSERT_CMPSTR(mongoc_uri_get_database(uri), "test");
   options = mongoc_uri_get_options(uri);
   ASSERT(options);
   ASSERT(bson_iter_init_find(&iter, options, "q"));
   ASSERT_CMPSTR(bson_iter_utf8(&iter, NULL), "1");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://local1,local2:999,local3?q=1");
   ASSERT(uri);
   hosts = mongoc_uri_get_hosts(uri);
   ASSERT(hosts);
   ASSERT(hosts->next);
   ASSERT(hosts->next->next);
   ASSERT(!hosts->next->next->next);
   ASSERT_CMPSTR(hosts->host, "local1");
   ASSERT_CMPINT(hosts->port, ==, 27017);
   ASSERT_CMPSTR(hosts->next->host, "local2");
   ASSERT_CMPINT(hosts->next->port, ==, 999);
   ASSERT_CMPSTR(hosts->next->next->host, "local3");
   ASSERT_CMPINT(hosts->next->next->port, ==, 27017);
   options = mongoc_uri_get_options(uri);
   ASSERT(options);
   ASSERT(bson_iter_init_find(&iter, options, "q"));
   ASSERT_CMPSTR(bson_iter_utf8(&iter, NULL), "1");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost:27017/?readPreferenceTags=dc:ny&readPreferenceTags=");
   ASSERT(uri);
   options = mongoc_uri_get_read_prefs(uri);
   ASSERT(options);
   ASSERT_CMPINT(bson_count_keys(options), ==, 2);
   ASSERT(bson_iter_init_find(&iter, options, "0"));
   ASSERT(BSON_ITER_HOLDS_DOCUMENT(&iter));
   ASSERT(bson_iter_recurse(&iter, &child));
   ASSERT(bson_iter_next(&child));
   ASSERT_CMPSTR(bson_iter_key(&child), "dc");
   ASSERT_CMPSTR(bson_iter_utf8(&child, NULL), "ny");
   ASSERT(!bson_iter_next(&child));
   ASSERT(bson_iter_next(&iter));
   ASSERT(BSON_ITER_HOLDS_DOCUMENT(&iter));
   ASSERT(bson_iter_recurse(&iter, &child));
   ASSERT(!bson_iter_next(&child));
   ASSERT(!bson_iter_next(&iter));
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost/a?slaveok=true&ssl=false&journal=true");
   options = mongoc_uri_get_options(uri);
   ASSERT(options);
   ASSERT_CMPINT(bson_count_keys(options), ==, 3);
   ASSERT(bson_iter_init(&iter, options));
   ASSERT(bson_iter_find_case(&iter, "slaveok"));
   ASSERT(BSON_ITER_HOLDS_BOOL(&iter));
   ASSERT(bson_iter_bool(&iter));
   ASSERT(bson_iter_find_case(&iter, "ssl"));
   ASSERT(BSON_ITER_HOLDS_BOOL(&iter));
   ASSERT(!bson_iter_bool(&iter));
   ASSERT(bson_iter_find_case(&iter, "journal"));
   ASSERT(BSON_ITER_HOLDS_BOOL(&iter));
   ASSERT(bson_iter_bool(&iter));
   ASSERT(!bson_iter_next(&iter));
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb:///tmp/mongodb-27017.sock/?ssl=false");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->host, "/tmp/mongodb-27017.sock");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb:///tmp/mongodb-27017.sock,localhost:27017/?ssl=false");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->host, "/tmp/mongodb-27017.sock");
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->next->host_and_port, "localhost:27017");
   ASSERT(!mongoc_uri_get_hosts(uri)->next->next);
   mongoc_uri_destroy(uri);

   /* should assign port numbers to correct hosts */
   uri = mongoc_uri_new("mongodb://host1,host2:30000/foo/");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->host_and_port, "host1:27017");
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->next->host_and_port, "host2:30000");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://localhost:27017,/tmp/mongodb-27017.sock/?ssl=false");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->host_and_port, "localhost:27017");
   ASSERT_CMPSTR(mongoc_uri_get_hosts(uri)->next->host, "/tmp/mongodb-27017.sock");
   ASSERT(!mongoc_uri_get_hosts(uri)->next->next);
   mongoc_uri_destroy(uri);

   /* should use the authSource over db when both are specified */
   uri = mongoc_uri_new("mongodb://christian:secret@localhost:27017/foo/?authSource=abcd");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_username(uri), "christian");
   ASSERT_CMPSTR(mongoc_uri_get_password(uri), "secret");
   ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "abcd");
   mongoc_uri_destroy(uri);

   /* should use the default auth source and mechanism */
   uri = mongoc_uri_new("mongodb://christian:secret@localhost:27017");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "admin");
   ASSERT(!mongoc_uri_get_auth_mechanism(uri));
   mongoc_uri_destroy(uri);

   /* should use the db when no authSource is specified */
   uri = mongoc_uri_new("mongodb://user:password@localhost/foo");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "foo");
   mongoc_uri_destroy(uri);

   /* should recognize an empty password */
   uri = mongoc_uri_new("mongodb://samantha:@localhost");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_username(uri), "samantha");
   ASSERT_CMPSTR(mongoc_uri_get_password(uri), "");
   mongoc_uri_destroy(uri);

   /* should recognize no password */
   uri = mongoc_uri_new("mongodb://christian@localhost:27017");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_username(uri), "christian");
   ASSERT(!mongoc_uri_get_password(uri));
   mongoc_uri_destroy(uri);

   /* should recognize a url escaped character in the username */
   uri = mongoc_uri_new("mongodb://christian%40realm:pwd@localhost:27017");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_username(uri), "christian@realm");
   mongoc_uri_destroy(uri);

   /* while you shouldn't do this, lets test for it */
   uri = mongoc_uri_new("mongodb://christian%40realm@localhost:27017/db%2ename");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_database(uri), "db.name");
   mongoc_uri_destroy(uri);
   uri = mongoc_uri_new("mongodb://christian%40realm@localhost:27017/db%2Ename");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_database(uri), "db.name");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://christian%40realm@localhost:27017/?abcd=%20");
   ASSERT(uri);
   options = mongoc_uri_get_options(uri);
   ASSERT(options);
   ASSERT(bson_iter_init_find(&iter, options, "abcd"));
   ASSERT(BSON_ITER_HOLDS_UTF8(&iter));
   ASSERT_CMPSTR(bson_iter_utf8(&iter, NULL), " ");
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://christian%40realm@[::6]:27017/?abcd=%20");
   ASSERT(uri);
   options = mongoc_uri_get_options(uri);
   ASSERT(options);
   ASSERT(bson_iter_init_find(&iter, options, "abcd"));
   ASSERT(BSON_ITER_HOLDS_UTF8(&iter));
   ASSERT_CMPSTR(bson_iter_utf8(&iter, NULL), " ");
   mongoc_uri_destroy(uri);

   /* GSSAPI-specific options */

   /* should recognize the GSSAPI mechanism, and use $external as source */
   uri = mongoc_uri_new("mongodb://user%40DOMAIN.COM:password@localhost/?authMechanism=GSSAPI");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_mechanism(uri), "GSSAPI");
   /*ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "$external");*/
   mongoc_uri_destroy(uri);

   /* use $external as source when db is specified */
   uri = mongoc_uri_new("mongodb://user%40DOMAIN.COM:password@localhost/foo/?authMechanism=GSSAPI");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "$external");
   mongoc_uri_destroy(uri);

   /* should not accept authSource other than $external */
   ASSERT(!mongoc_uri_new("mongodb://user%40DOMAIN.COM:password@localhost/foo/?authMechanism=GSSAPI&authSource=bar"));

   /* should accept authMechanismProperties */
   uri = mongoc_uri_new("mongodb://user%40DOMAIN.COM:password@localhost/?authMechanism=GSSAPI"
                        "&authMechanismProperties=SERVICE_NAME:other,CANONICALIZE_HOST_NAME:true");
   ASSERT(uri);
   credentials = mongoc_uri_get_credentials(uri);
   ASSERT(credentials);
   ASSERT(mongoc_uri_get_mechanism_properties(uri, &properties));
   assert (bson_iter_init_find_case (&iter, &properties, "SERVICE_NAME") &&
           BSON_ITER_HOLDS_UTF8 (&iter) &&
           (0 == strcmp (bson_iter_utf8 (&iter, NULL), "other")));
   assert (bson_iter_init_find_case (&iter, &properties, "CANONICALIZE_HOST_NAME") &&
           BSON_ITER_HOLDS_UTF8 (&iter) &&
           (0 == strcmp (bson_iter_utf8 (&iter, NULL), "true")));
   mongoc_uri_destroy(uri);

   /* reverse order of arguments to ensure parsing still succeeds */
   uri = mongoc_uri_new("mongodb://user@localhost/"
                        "?authMechanismProperties=SERVICE_NAME:other"
                        "&authMechanism=GSSAPI");
   ASSERT(uri);
   mongoc_uri_destroy(uri);

   /* deprecated gssapiServiceName option */
   uri = mongoc_uri_new("mongodb://christian%40realm.cc@localhost:27017/?authMechanism=GSSAPI&gssapiServiceName=blah");
   ASSERT(uri);
   options = mongoc_uri_get_options(uri);
   ASSERT(options);
   assert (0 == strcmp (mongoc_uri_get_auth_mechanism (uri), "GSSAPI"));
   assert (0 == strcmp (mongoc_uri_get_username (uri), "christian@realm.cc"));
   assert (bson_iter_init_find_case (&iter, options, "gssapiServiceName") &&
           BSON_ITER_HOLDS_UTF8 (&iter) &&
           (0 == strcmp (bson_iter_utf8 (&iter, NULL), "blah")));
   mongoc_uri_destroy(uri);

   /* MONGODB-CR */

   /* should recognize this mechanism */
   uri = mongoc_uri_new("mongodb://user@localhost/?authMechanism=MONGODB-CR");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_mechanism(uri), "MONGODB-CR");
   mongoc_uri_destroy(uri);

   /* X509 */

   /* should recognize this mechanism, and use $external as the source */
   uri = mongoc_uri_new("mongodb://user@localhost/?authMechanism=MONGODB-X509");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_mechanism(uri), "MONGODB-X509");
   /*ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "$external");*/
   mongoc_uri_destroy(uri);

   /* use $external as source when db is specified */
   uri = mongoc_uri_new("mongodb://CN%3DmyName%2COU%3DmyOrgUnit%2CO%3DmyOrg%2CL%3DmyLocality"
                        "%2CST%3DmyState%2CC%3DmyCountry@localhost/foo/?authMechanism=MONGODB-X509");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_source(uri), "$external");
   mongoc_uri_destroy(uri);

   /* should not accept authSource other than $external */
   ASSERT(!mongoc_uri_new("mongodb://CN%3DmyName%2COU%3DmyOrgUnit%2CO%3DmyOrg%2CL%3DmyLocality"
                          "%2CST%3DmyState%2CC%3DmyCountry@localhost/foo/?authMechanism=MONGODB-X509&authSource=bar"));

   /* should recognize the encoded username */
   uri = mongoc_uri_new("mongodb://CN%3DmyName%2COU%3DmyOrgUnit%2CO%3DmyOrg%2CL%3DmyLocality"
                        "%2CST%3DmyState%2CC%3DmyCountry@localhost/?authMechanism=MONGODB-X509");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_username(uri), "CN=myName,OU=myOrgUnit,O=myOrg,L=myLocality,ST=myState,C=myCountry");
   mongoc_uri_destroy(uri);

   /* PLAIN */

   /* should recognize this mechanism */
   uri = mongoc_uri_new("mongodb://user@localhost/?authMechanism=PLAIN");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_mechanism(uri), "PLAIN");
   mongoc_uri_destroy(uri);

   /* SCRAM-SHA1 */

   /* should recognize this mechanism */
   uri = mongoc_uri_new("mongodb://user@localhost/?authMechanism=SCRAM-SHA1");
   ASSERT(uri);
   ASSERT_CMPSTR(mongoc_uri_get_auth_mechanism(uri), "SCRAM-SHA1");
   mongoc_uri_destroy(uri);
}

static void
test_mongoc_host_list_from_string (void)
{
   mongoc_host_list_t host_list = { 0 };

   ASSERT(_mongoc_host_list_from_string(&host_list, "localhost:27019"));
   ASSERT(!strcmp(host_list.host_and_port, "localhost:27019"));
   ASSERT(!strcmp(host_list.host, "localhost"));
   ASSERT(host_list.port == 27019);
   ASSERT(host_list.family == AF_INET);
   ASSERT(!host_list.next);
}


static void
test_mongoc_uri_new_for_host_port (void)
{
   mongoc_uri_t *uri;

   uri = mongoc_uri_new_for_host_port("uber", 555);
   ASSERT(uri);
   ASSERT(!strcmp("uber", mongoc_uri_get_hosts(uri)->host));
   ASSERT(!strcmp("uber:555", mongoc_uri_get_hosts(uri)->host_and_port));
   ASSERT(555 == mongoc_uri_get_hosts(uri)->port);
   mongoc_uri_destroy(uri);
}


static void
test_mongoc_uri_unescape (void)
{
#define ASSERT_URIDECODE_STR(_s, _e) \
   do { \
      char *str = mongoc_uri_unescape(_s); \
      ASSERT(!strcmp(str, _e)); \
      bson_free(str); \
   } while (0)
#define ASSERT_URIDECODE_FAIL(_s) \
   do { \
      char *str = mongoc_uri_unescape(_s); \
      ASSERT(!str); \
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


typedef struct
{
   const char *uri;
   bool        parses;
   int32_t     w;
   const char *wtag;
} write_concern_test;


static void
test_mongoc_uri_write_concern (void)
{
   const mongoc_write_concern_t *wr;
   mongoc_uri_t *uri;
   const write_concern_test *t;
   int i;
   static const write_concern_test tests [] = {
      { "mongodb://localhost/?safe=false", true, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED },
      { "mongodb://localhost/?safe=true", true, MONGOC_WRITE_CONCERN_W_DEFAULT },
      { "mongodb://localhost/?w=-1", true, MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED },
      { "mongodb://localhost/?w=0", true, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED },
      { "mongodb://localhost/?w=1", true, MONGOC_WRITE_CONCERN_W_DEFAULT },
      { "mongodb://localhost/?w=2", true, 2 },
      { "mongodb://localhost/?w=majority", true, MONGOC_WRITE_CONCERN_W_MAJORITY },
      { "mongodb://localhost/?w=10", true, 10 },
      { "mongodb://localhost/?w=", true, MONGOC_WRITE_CONCERN_W_DEFAULT },
      { "mongodb://localhost/?w=mytag", true, MONGOC_WRITE_CONCERN_W_TAG, "mytag" },
      { "mongodb://localhost/?w=mytag&safe=false", true, MONGOC_WRITE_CONCERN_W_TAG, "mytag" },
      { "mongodb://localhost/?w=1&safe=false", true, MONGOC_WRITE_CONCERN_W_DEFAULT },
      { NULL }
   };

   for (i = 0; tests [i].uri; i++) {
      t = &tests [i];

      uri = mongoc_uri_new (t->uri);
      if (t->parses) {
         assert (uri);
      } else {
         assert (!uri);
         continue;
      }

      wr = mongoc_uri_get_write_concern (uri);
      assert (wr);

      assert (t->w == mongoc_write_concern_get_w (wr));

      if (t->wtag) {
         assert (0 == strcmp (t->wtag, mongoc_write_concern_get_wtag (wr)));
      }

      mongoc_uri_destroy (uri);
   }
}


void
test_uri_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Uri/new", test_mongoc_uri_new);
   TestSuite_Add (suite, "/Uri/new_for_host_port", test_mongoc_uri_new_for_host_port);
   TestSuite_Add (suite, "/Uri/unescape", test_mongoc_uri_unescape);
   TestSuite_Add (suite, "/Uri/write_concern", test_mongoc_uri_write_concern);
   TestSuite_Add (suite, "/HostList/from_string", test_mongoc_host_list_from_string);
}

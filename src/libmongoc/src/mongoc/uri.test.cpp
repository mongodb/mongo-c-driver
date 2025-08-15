#include <mongoc/mongoc.h>

#include <catch2-shim.hpp>


inline std::string
operator""_str(const char *s, std::size_t len)
{
   return std::string(s, len);
}

TEST_CASE("/foo/more-cases")
{
   CHECK_FALSE(false);
   CHECK(true);
   CHECK(42 == 42);
   CHECK_FALSE(42 != 42);
}

TEST_CASE("/Uri/bad-rejected")
{
   std::string bad_uris[] = {
      "mongodb://",
      "mongodb://\x80",
      "mongodb://localhost/\x80",
      "mongodb://localhost:\x80/",
      "mongodb://localhost/?ipv6=\x80",
      "mongodb://localhost/?foo=\x80",
      "mongodb://localhost/?\x80=bar",
      "mongodb://\x80:pass@localhost",
      "mongodb://user:\x80@localhost",
      "mongodb://user%40DOMAIN.COM:password@localhost/?authMechanism=\x80",
      "mongodb://user%40DOMAIN.COM:password@localhost/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_NAME:\x80",
      "mongodb://user%40DOMAIN.COM:password@localhost/?authMechanism=GSSAPI&authMechanismProperties=\x80:mongodb",
      "mongodb://::",
      "mongodb://[::1]::27017/",
      "mongodb://localhost::27017",
      "mongodb://localhost,localhost::",
      "mongodb://local1,local2,local3/d?k",
      "",
      "mongodb://,localhost:27017",
      "mongodb://localhost:27017,,b",
      "mongo://localhost:27017",
      "mongodb://localhost::27017",
      "mongodb://localhost::27017/",
      "mongodb://localhost::27017,abc",
      "mongodb://localhost:-1",
      "mongodb://localhost:65536",
      "mongodb://localhost:foo",
      "mongodb://localhost:65536/",
      "mongodb://localhost:0/",
      "mongodb://[::1%lo0]",
      "mongodb://[::1]:-1",
      "mongodb://[::1]:foo",
      "mongodb://[::1]:65536",
      "mongodb://[::1]:65536/",
      "mongodb://[::1]:0/",
      "mongodb://localhost:27017/test?replicaset=",
      "mongodb://local1,local2/?directConnection=true",
      "mongodb+srv://local1/?directConnection=true",

      // Name too long
      ("mongodb://"
       "localhost?appName="
       "AppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLon"
       "gAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLongAppnameTooLong"),
   };
   for (auto uri_string : bad_uris) {
      CAPTURE(uri_string);
      mongoc_uri_t *got = mongoc_uri_new(uri_string.c_str());
      CHECK(got == nullptr);
      // Free the URI in case we accidentally accepted a bad one (prevent leak errors from
      // cluttering error output)
      mongoc_uri_destroy(got);
   }
}

TEST_CASE("/Uri/multi-host-ipv6")
{
   auto uri = mongoc_uri_new("mongodb://[::1]:27888,[::2]:27999/?ipv6=true&safe=true");
   REQUIRE(uri);
   auto hosts = mongoc_uri_get_hosts(uri);
   // Check first host:
   REQUIRE(hosts);
   CHECK(hosts->host == "::1"_str);
   CHECK(hosts->port == 27888);
   CHECK(hosts->host_and_port == "[::1]:27888"_str);
   // Check second host:
   REQUIRE(hosts->next);
   CHECK(hosts->next->host == "::2"_str);
   CHECK(hosts->next->port == 27999);
   CHECK(hosts->next->host_and_port == "[::2]:27999"_str);
   // No third host:
   CHECK_FALSE(hosts->next->next);
   mongoc_uri_destroy(uri);
}

TEST_CASE("/Uri/host-ipv6-with-scope")
{
   auto uri = mongoc_uri_new("mongodb://[::1%25lo0]");
   REQUIRE(uri);
   auto hosts = mongoc_uri_get_hosts(uri);
   REQUIRE(hosts);
   CHECK(hosts->host == "::1%lo0"_str);
   CHECK(hosts->port == 27017);
   CHECK(hosts->host_and_port == "[::1%lo0]:27017"_str);
   mongoc_uri_destroy(uri);
}

TEST_CASE("/Uri/socket-path-host")
{
   auto uri = mongoc_uri_new("mongodb://%2ftmp%2fmongodb-27017.sock/?");
   REQUIRE(uri);
   auto hosts = mongoc_uri_get_hosts(uri);
   REQUIRE(hosts);
   CAPTURE(hosts->host);
   CHECK(hosts->host == "/tmp/mongodb-27017.sock"_str);
   mongoc_uri_destroy(uri);
}

TEST_CASE("/Uri/normalize-host")
{
   auto uri = mongoc_uri_new("mongodb://cRaZyHoStNaMe");
   REQUIRE(uri);
   auto hosts = mongoc_uri_get_hosts(uri);
   REQUIRE(hosts);
   // Hostname is converted to all-lowercase
   CHECK(hosts->host == "crazyhostname"_str);
   mongoc_uri_destroy(uri);
}

TEST_CASE("/Uri/simple")
{
   auto uri = mongoc_uri_new("mongodb://localhost:27017/test?replicaSet=foo");
   REQUIRE(uri);
   auto hosts = mongoc_uri_get_hosts(uri);
   REQUIRE(hosts);
   CHECK(hosts->host == "localhost"_str);
   CHECK(hosts->port == 27017);
   auto db = mongoc_uri_get_database(uri);
   REQUIRE(db);
   CHECK(db == "test"_str);
   auto repl = mongoc_uri_get_replica_set(uri);
   REQUIRE(repl);
   CHECK(repl == "foo"_str);
   mongoc_uri_destroy(uri);
}

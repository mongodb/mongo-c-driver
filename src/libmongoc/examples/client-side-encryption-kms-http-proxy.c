// Demonstrates how to route KMS network traffic through an HTTP CONNECT proxy using
// mongoc_kms_connect_callback_t. This is useful when outbound network access is only permitted through a proxy
// (e.g. in a locked-down data center or CI environment).
//
// This example requires:
// - A KMS provider that is reached over the network (this example uses "aws"). The "local" KMS provider never
//   opens a network connection, so it never invokes the KMS connect callback.
// - An HTTP CONNECT proxy reachable from this process. Set MONGOC_KMS_PROXY_HOST / MONGOC_KMS_PROXY_PORT to
//   point at it (defaults to 127.0.0.1:3128).
// - AWS credentials and a customer master key (CMK) ARN, provided via AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY,
//   and AWS_KMS_KEY_ARN.

#include <mongoc/mongoc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL(...)                                           \
   fprintf(stderr, "Error [%s:%d]:\n", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__);                            \
   fprintf(stderr, "\n");                                   \
   abort();

// `init_bson` creates BSON from JSON. Aborts on error. Use the `BSON_STR()` macro to avoid quotes.
#define init_bson(bson, json)                           \
   if (!bson_init_from_json(&bson, json, -1, &error)) { \
      FAIL("Failed to create BSON: %s", error.message); \
   }

// `getenv_or` returns the value of environment variable `name`, or `default_value` if it is unset.
static const char *
getenv_or(const char *name, const char *default_value)
{
   const char *value = getenv(name);
   return value ? value : default_value;
}

// Configuration for the HTTP CONNECT proxy, passed as the `user_data` of a mongoc_kms_connect_callback_t.
typedef struct {
   const char *proxy_host;
   uint16_t proxy_port;
} kms_proxy_config_t;

// Opens a plain TCP connection to `host`:`port`. Returns a connected mongoc_stream_t, or NULL and sets `error`.
static mongoc_stream_t *
tcp_connect(const char *host, uint16_t port, int32_t connecttimeoutms, bson_error_t *error)
{
   struct addrinfo hints = {0};
   hints.ai_socktype = SOCK_STREAM;

   char portstr[8];
   bson_snprintf(portstr, sizeof(portstr), "%hu", port);

   struct addrinfo *result = NULL;
   if (0 != getaddrinfo(host, portstr, &hints, &result)) {
      bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_NAME_RESOLUTION, "Failed to resolve %s", host);
      return NULL;
   }

   int64_t expire_at = bson_get_monotonic_time() + ((int64_t)connecttimeoutms * 1000);
   mongoc_socket_t *sock = NULL;
   for (struct addrinfo *rp = result; rp; rp = rp->ai_next) {
      sock = mongoc_socket_new(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (!sock) {
         continue;
      }
      if (0 == mongoc_socket_connect(sock, rp->ai_addr, (mongoc_socklen_t)rp->ai_addrlen, expire_at)) {
         break;
      }
      mongoc_socket_destroy(sock);
      sock = NULL;
   }
   freeaddrinfo(result);

   if (!sock) {
      bson_set_error(
         error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT, "Failed to connect to %s:%hu", host, port);
      return NULL;
   }

   return mongoc_stream_socket_new(sock);
}

// BEGIN:mongoc_kms_connect_callback_fn_t
// A mongoc_kms_connect_callback_fn_t implementation. Connects to an HTTP CONNECT proxy and asks it to open a
// tunnel to the KMS endpoint at `host`:`port`. The driver applies TLS to the returned stream itself, so this
// callback only needs to hand back a connected, un-encrypted tunnel.
static mongoc_stream_t *
kms_connect_via_http_proxy(
   const char *host, uint16_t port, int32_t connecttimeoutms, void *user_data, bson_error_t *error)
{
   const kms_proxy_config_t *config = (kms_proxy_config_t *)user_data;

   mongoc_stream_t *proxy_stream = tcp_connect(config->proxy_host, config->proxy_port, connecttimeoutms, error);
   if (!proxy_stream) {
      return NULL;
   }

   // Ask the proxy to open a tunnel to the KMS endpoint:
   char req[512];
   int req_len =
      bson_snprintf(req, sizeof(req), "CONNECT %s:%hu HTTP/1.1\r\nHost: %s:%hu\r\n\r\n", host, port, host, port);
   if (mongoc_stream_write(proxy_stream, req, (size_t)req_len, connecttimeoutms) != req_len) {
      bson_set_error(
         error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT, "Failed to send CONNECT request to proxy");
      mongoc_stream_destroy(proxy_stream);
      return NULL;
   }

   // Read the proxy's response headers, one byte at a time, until the blank line that ends them:
   char resp[1024] = {0};
   size_t resp_len = 0;
   while (resp_len + 1 < sizeof(resp)) {
      ssize_t r = mongoc_stream_read(proxy_stream, resp + resp_len, 1, 1 /* min_bytes */, connecttimeoutms);
      if (r <= 0) {
         bson_set_error(
            error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT, "Failed to read CONNECT response from proxy");
         mongoc_stream_destroy(proxy_stream);
         return NULL;
      }
      resp_len += (size_t)r;
      if (resp_len >= 4 && 0 == memcmp(resp + resp_len - 4, "\r\n\r\n", 4)) {
         break;
      }
   }

   if (!strstr(resp, " 200 ")) {
      bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT, "Proxy CONNECT failed: %s", resp);
      mongoc_stream_destroy(proxy_stream);
      return NULL;
   }

   // `proxy_stream` now delivers raw bytes to `host`:`port`. The driver wraps it with TLS before sending any KMS
   // requests.
   return proxy_stream;
}
// END:mongoc_kms_connect_callback_fn_t

int
main(void)
{
   bson_error_t error;

   // The key vault collection stores encrypted data keys:
   const char *keyvault_db_name = "keyvault";
   const char *keyvault_coll_name = "datakeys";

   const char *uri = "mongodb://localhost/?appname=client-side-encryption-kms-http-proxy";

   mongoc_init();

   // Create client:
   mongoc_client_t *client = mongoc_client_new(uri);
   if (!client) {
      FAIL("Failed to create client");
   }

   // Configure the KMS provider used to encrypt data keys. AWS KMS is used here because it is reached over the
   // network; the HTTP CONNECT proxy only comes into play for KMS providers that make network requests.
   bson_t kms_providers;
   {
      const char *access_key_id = getenv("AWS_ACCESS_KEY_ID");
      const char *secret_access_key = getenv("AWS_SECRET_ACCESS_KEY");
      if (!access_key_id || !secret_access_key) {
         FAIL("Set the AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY environment variables");
      }
      char *as_json = bson_strdup_printf(
         BSON_STR({"aws" : {"accessKeyId" : "%s", "secretAccessKey" : "%s"}}), access_key_id, secret_access_key);
      init_bson(kms_providers, as_json);
      bson_free(as_json);
   }

   // Set up key vault collection:
   {
      mongoc_collection_t *coll = mongoc_client_get_collection(client, keyvault_db_name, keyvault_coll_name);
      mongoc_collection_drop(coll, NULL); // Clear pre-existing data.

      // Create index to ensure keys have unique keyAltNames:
      bson_t index_keys, index_opts;
      init_bson(index_keys, BSON_STR({"keyAltNames" : 1}));
      init_bson(index_opts,
                BSON_STR({"unique" : true, "partialFilterExpression" : {"keyAltNames" : {"$exists" : true}}}));
      mongoc_index_model_t *index_model = mongoc_index_model_new(&index_keys, &index_opts);
      if (!mongoc_collection_create_indexes_with_opts(
             coll, &index_model, 1, NULL /* opts */, NULL /* reply */, &error)) {
         FAIL("Failed to create index: %s", error.message);
      }

      mongoc_index_model_destroy(index_model);
      bson_destroy(&index_opts);
      bson_destroy(&index_keys);
      mongoc_collection_destroy(coll);
   }

   // BEGIN:mongoc_client_encryption_opts_set_kms_connect_callback
   // Configure the HTTP CONNECT proxy that KMS traffic is routed through:
   kms_proxy_config_t proxy_config = {
      .proxy_host = getenv_or("MONGOC_KMS_PROXY_HOST", "127.0.0.1"),
      .proxy_port = (uint16_t)atoi(getenv_or("MONGOC_KMS_PROXY_PORT", "3128")),
   };
   mongoc_kms_connect_callback_t *connect_callback =
      mongoc_kms_connect_callback_new_with_user_data(kms_connect_via_http_proxy, &proxy_config);

   // Create ClientEncryption object:
   mongoc_client_encryption_t *client_encryption;
   {
      mongoc_client_encryption_opts_t *ce_opts = mongoc_client_encryption_opts_new();
      mongoc_client_encryption_opts_set_kms_providers(ce_opts, &kms_providers);
      mongoc_client_encryption_opts_set_keyvault_namespace(ce_opts, keyvault_db_name, keyvault_coll_name);
      mongoc_client_encryption_opts_set_keyvault_client(ce_opts, client);
      // Route all KMS network traffic through the HTTP CONNECT proxy:
      mongoc_client_encryption_opts_set_kms_connect_callback(ce_opts, connect_callback);
      client_encryption = mongoc_client_encryption_new(ce_opts, &error);
      if (!client_encryption) {
         FAIL("Failed to create ClientEncryption: %s", error.message);
      }
      mongoc_client_encryption_opts_destroy(ce_opts);
   }

   // The callback object is copied by `mongoc_client_encryption_opts_set_kms_connect_callback`, so it may be
   // destroyed as soon as it has been set on every options object that needs it. `proxy_config` must remain valid
   // for as long as `client_encryption` may invoke the callback, since only a pointer to it was stored.
   mongoc_kms_connect_callback_destroy(connect_callback);
   // END:mongoc_client_encryption_opts_set_kms_connect_callback

   // Create a data key. The driver invokes `kms_connect_via_http_proxy` to reach AWS KMS through the proxy:
   bson_value_t datakey_id;
   {
      mongoc_client_encryption_datakey_opts_t *dk_opts = mongoc_client_encryption_datakey_opts_new();

      char *masterkey_json = bson_strdup_printf(BSON_STR({"region" : "us-east-1", "key" : "%s"}),
                                                getenv_or("AWS_KMS_KEY_ARN", "<AWS KMS CMK ARN>"));
      bson_t masterkey;
      init_bson(masterkey, masterkey_json);
      bson_free(masterkey_json);
      mongoc_client_encryption_datakey_opts_set_masterkey(dk_opts, &masterkey);

      if (!mongoc_client_encryption_create_datakey(client_encryption, "aws", dk_opts, &datakey_id, &error)) {
         FAIL("Failed to create data key: %s", error.message);
      }

      bson_destroy(&masterkey);
      mongoc_client_encryption_datakey_opts_destroy(dk_opts);
   }

   printf("Created data key via the KMS HTTP CONNECT proxy\n");

   bson_value_destroy(&datakey_id);
   mongoc_client_encryption_destroy(client_encryption);
   bson_destroy(&kms_providers);
   mongoc_client_destroy(client);
   mongoc_cleanup();
   return 0;
}

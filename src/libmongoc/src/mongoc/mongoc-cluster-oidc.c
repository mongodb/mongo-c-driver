#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"
#include "mongoc-oidc-callback.h"

struct _mongoc_oidc_callback_params_t {
    int64_t callback_timeout_ms;
    int64_t version;
};

struct _mongoc_oidc_credential_t {
    char *access_token;
    int64_t expires_in_seconds;
};

int64_t
mongoc_oidc_callback_params_get_timeout_ms(mongoc_oidc_callback_params_t* callback_params)
{
    return callback_params->callback_timeout_ms;
}

int64_t
mongoc_oidc_callback_params_get_version(mongoc_oidc_callback_params_t* callback_params)
{
    return callback_params->version;
}

char *
mongoc_oidc_credential_get_access_token(mongoc_oidc_credential_t *credential)
{
    return credential->access_token;
}

int64_t
mongoc_oidc_credential_get_expires_in_seconds(mongoc_oidc_credential_t *credential)
{
    return credential->expires_in_seconds;
}

bool
_mongoc_cluster_auth_node_oidc (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error)
{
   bool ret = true;
   mongoc_oidc_callback_params_t *params = NULL;
   mongoc_oidc_credential_t *creds = NULL;

   BSON_ASSERT (cluster);
   BSON_ASSERT (cluster->client);
   BSON_ASSERT (cluster->client->oidc_callback);
   BSON_ASSERT (stream);

   /*
    * 1) Call callback function with params, store the result as the OIDC Token
    */

   // creds = cluster->client->oidc_callback(params);
   return ret;
}

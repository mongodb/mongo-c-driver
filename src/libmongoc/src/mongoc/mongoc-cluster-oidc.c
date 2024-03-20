#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mongoc-cluster-private.h"

struct _mongoc_oidc_callback_t {
    int64_t callback_timeout_ms;
    int64_t version;
};

typedef struct _mongoc_oidc_callback_t mongoc_oidc_callback_t;

/*
 */
bool
_mongoc_cluster_auth_node_oidc (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error)
{
   bool ret = true;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);

   return ret;
}

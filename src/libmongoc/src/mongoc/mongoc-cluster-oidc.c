#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mongoc-cluster-private.h"

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

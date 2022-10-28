/**
 * Copyright 2022 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SERVICE_GCP_H_INCLUDED
#define SERVICE_GCP_H_INCLUDED

#include "mongoc-prelude.h"

#include <mongoc/mongoc.h>

#include <mongoc/mongoc-http-private.h>
#include <mcd-time.h>

/**
 * @brief A GCP access token obtained from the GCP metadata server
 */
typedef struct gcp_service_account_token {
   /// The access token string
   char *access_token;
   // The HTTP type of the token
   char *token_type;
   // The duration after which it will the token will expires. This is
   // relative to the "issue time" of the token.
   // mcd_duration expires_in;
} gcp_service_account_token;

/**
 * @brief A GCP request
 */
typedef struct gcp_request {
   /// The underlying HTTP request object to be sent
   mongoc_http_request_t req;
   char *_owned_path;
   char *_owned_host;
   char *_owned_headers;
} gcp_request;


void
gcp_request_init (gcp_request *req,
                  const char *const opt_host,
                  int opt_port,
                  const char *const opt_extra_headers);

void
gcp_request_destroy (gcp_request *req);

void
gcp_access_token_destroy (gcp_service_account_token *token);


/**
 * @brief Try to parse a GCP access token from a metadata JSON response
 *
 * @param out The token to initialize. Should be uninitialized. Must later be
 * destroyed by the caller.
 * @param json The JSON string body
 * @param len The length of 'body'
 * @param error An output parameter for errors
 * @retval true If 'out' was successfully initialized to a token.
 * @retval false Otherwise
 *
 * @note The 'out' token must later be given to @ref
 * gcp_access_token_destroy
 */
bool
gcp_access_token_try_parse_from_json (gcp_service_account_token *out,
                                      const char *json,
                                      int len,
                                      bson_error_t *error);

bool
gcp_access_token_from_api (gcp_service_account_token *out,
                           const char *opt_host,
                           int opt_port,
                           const char *opt_extra_headers,
                           bson_error_t *error);

#endif /* SERVICE_GCP_H */

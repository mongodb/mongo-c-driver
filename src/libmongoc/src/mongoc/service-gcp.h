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

/**
 * @brief A GCP access token obtained from the GCP API
 */
typedef struct gcp_service_account_token {
   /// The access token string
   char *access_token;
   /// The resource of the token (the Azure resource for which it is valid)
   //    char *resource;
   //    /// The HTTP type of the token
   //    char *token_type;
   //    /// The duration after which it will the token will expires. This is
   //    relative
   //    /// to the "issue time" of the token.
   // gcp_duration expires_in;
} gcp_service_account_token;

#endif /* SERVICE_GCP_H */

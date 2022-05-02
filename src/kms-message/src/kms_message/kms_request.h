/*
 * Copyright 2018-present MongoDB, Inc.
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

#ifndef KMS_REQUEST_H
#define KMS_REQUEST_H

#include "bson-compat.h"
#include "kms_message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _kms_request_t kms_request_t;

KMS_MSG_EXPORT (kms_request_t *)
kms_request_new (const char *method,
                 const char *path_and_query,
                 const kms_request_opt_t *opt);
KMS_MSG_EXPORT (void)
kms_request_destroy (kms_request_t *request);
KMS_MSG_EXPORT (const char *)
kms_request_get_error (kms_request_t *request);
KMS_MSG_EXPORT (bool)
kms_request_set_date (kms_request_t *request, const struct tm *tm);
KMS_MSG_EXPORT (bool)
kms_request_set_region (kms_request_t *request, const char *region);
KMS_MSG_EXPORT (bool)
kms_request_set_service (kms_request_t *request, const char *service);
KMS_MSG_EXPORT (bool)
kms_request_set_access_key_id (kms_request_t *request, const char *akid);
KMS_MSG_EXPORT (bool)
kms_request_set_secret_key (kms_request_t *request, const char *key);
KMS_MSG_EXPORT (bool)
kms_request_add_header_field (kms_request_t *request,
                              const char *field_name,
                              const char *value);
KMS_MSG_EXPORT (bool)
kms_request_append_header_field_value (kms_request_t *request,
                                       const char *value,
                                       size_t len);
KMS_MSG_EXPORT (bool)
kms_request_append_payload (kms_request_t *request,
                            const char *payload,
                            size_t len);
KMS_MSG_EXPORT (char *)
kms_request_get_canonical (kms_request_t *request);

KMS_MSG_EXPORT (const char *)
kms_request_get_canonical_header (kms_request_t *request, const char *header);

KMS_MSG_EXPORT (char *)
kms_request_get_string_to_sign (kms_request_t *request);
KMS_MSG_EXPORT (bool)
kms_request_get_signing_key (kms_request_t *request, unsigned char *key);
KMS_MSG_EXPORT (char *)
kms_request_get_signature (kms_request_t *request);
KMS_MSG_EXPORT (char *)
kms_request_get_signed (kms_request_t *request);
KMS_MSG_EXPORT (void)
kms_request_free_string (char *ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* KMS_REQUEST_H */

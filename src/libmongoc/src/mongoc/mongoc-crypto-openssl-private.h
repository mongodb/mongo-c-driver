/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <mongoc/mongoc-prelude.h>

#include <mongoc/mongoc-config.h>

#include <bson/bson.h>

#ifdef MONGOC_ENABLE_CRYPTO_LIBCRYPTO

#ifndef MONGOC_CRYPTO_OPENSSL_PRIVATE_H
#define MONGOC_CRYPTO_OPENSSL_PRIVATE_H

#include <mongoc/mongoc-crypto-private.h>

BSON_BEGIN_DECLS

bool
mongoc_crypto_openssl_pbkdf2_hmac_sha1 (mongoc_crypto_t *crypto,
                                        const char *password,
                                        size_t password_len,
                                        const uint8_t *salt,
                                        size_t salt_len,
                                        uint32_t iterations,
                                        size_t output_len,
                                        unsigned char *output);

void
mongoc_crypto_openssl_hmac_sha1 (mongoc_crypto_t *crypto,
                                 const void *key,
                                 int key_len,
                                 const unsigned char *data,
                                 int data_len,
                                 unsigned char *hmac_out);

bool
mongoc_crypto_openssl_sha1 (mongoc_crypto_t *crypto,
                            const unsigned char *input,
                            const size_t input_len,
                            unsigned char *hash_out);

bool
mongoc_crypto_openssl_pbkdf2_hmac_sha256 (mongoc_crypto_t *crypto,
                                          const char *password,
                                          size_t password_len,
                                          const uint8_t *salt,
                                          size_t salt_len,
                                          uint32_t iterations,
                                          size_t output_len,
                                          unsigned char *output);

void
mongoc_crypto_openssl_hmac_sha256 (mongoc_crypto_t *crypto,
                                   const void *key,
                                   int key_len,
                                   const unsigned char *data,
                                   int data_len,
                                   unsigned char *hmac_out);

bool
mongoc_crypto_openssl_sha256 (mongoc_crypto_t *crypto,
                              const unsigned char *input,
                              const size_t input_len,
                              unsigned char *hash_out);

BSON_END_DECLS
#endif /* MONGOC_CRYPTO_OPENSSL_PRIVATE_H */
#endif /* MONGOC_ENABLE_CRYPTO_LIBCRYPTO */

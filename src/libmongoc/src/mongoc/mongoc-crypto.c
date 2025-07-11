/* Copyright 2009-present MongoDB, Inc.
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

#include <mongoc/mongoc-config.h>

#ifdef MONGOC_ENABLE_CRYPTO

#include <mongoc/mongoc-crypto-private.h>

#include <mongoc/mongoc-log.h>

#include <bson/bson.h>
#if defined(MONGOC_ENABLE_CRYPTO_LIBCRYPTO)
#include <mongoc/mongoc-crypto-openssl-private.h>
#elif defined(MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO)
#include <mongoc/mongoc-crypto-common-crypto-private.h>
#elif defined(MONGOC_ENABLE_CRYPTO_CNG)
#include <mongoc/mongoc-crypto-cng-private.h>
#endif

void
mongoc_crypto_init (mongoc_crypto_t *crypto, mongoc_crypto_hash_algorithm_t algo)
{
   crypto->pbkdf = NULL;
   crypto->hmac = NULL;
   crypto->hash = NULL;
   if (algo == MONGOC_CRYPTO_ALGORITHM_SHA_1) {
#ifdef MONGOC_ENABLE_CRYPTO_LIBCRYPTO
      crypto->pbkdf = mongoc_crypto_openssl_pbkdf2_hmac_sha1;
      crypto->hmac = mongoc_crypto_openssl_hmac_sha1;
      crypto->hash = mongoc_crypto_openssl_sha1;
#elif defined(MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO)
      crypto->pbkdf = mongoc_crypto_common_crypto_pbkdf2_hmac_sha1;
      crypto->hmac = mongoc_crypto_common_crypto_hmac_sha1;
      crypto->hash = mongoc_crypto_common_crypto_sha1;
#elif defined(MONGOC_ENABLE_CRYPTO_CNG)
      crypto->pbkdf = mongoc_crypto_cng_pbkdf2_hmac_sha1;
      crypto->hmac = mongoc_crypto_cng_hmac_sha1;
      crypto->hash = mongoc_crypto_cng_sha1;
#endif
   } else if (algo == MONGOC_CRYPTO_ALGORITHM_SHA_256) {
#ifdef MONGOC_ENABLE_CRYPTO_LIBCRYPTO
      crypto->pbkdf = mongoc_crypto_openssl_pbkdf2_hmac_sha256;
      crypto->hmac = mongoc_crypto_openssl_hmac_sha256;
      crypto->hash = mongoc_crypto_openssl_sha256;
#elif defined(MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO)
      crypto->pbkdf = mongoc_crypto_common_crypto_pbkdf2_hmac_sha256;
      crypto->hmac = mongoc_crypto_common_crypto_hmac_sha256;
      crypto->hash = mongoc_crypto_common_crypto_sha256;
#elif defined(MONGOC_ENABLE_CRYPTO_CNG)
      crypto->pbkdf = mongoc_crypto_cng_pbkdf2_hmac_sha256;
      crypto->hmac = mongoc_crypto_cng_hmac_sha256;
      crypto->hash = mongoc_crypto_cng_sha256;
#endif
   }
   BSON_ASSERT (crypto->pbkdf);
   BSON_ASSERT (crypto->hmac);
   BSON_ASSERT (crypto->hash);
   crypto->algorithm = algo;
}

bool
mongoc_crypto_pbkdf (mongoc_crypto_t *crypto,
                     const char *password,
                     size_t password_len,
                     const uint8_t *salt,
                     size_t salt_len,
                     uint32_t iterations,
                     size_t output_len,
                     unsigned char *output)
{
   return crypto->pbkdf (crypto, password, password_len, salt, salt_len, iterations, output_len, output);
}

void
mongoc_crypto_hmac (mongoc_crypto_t *crypto,
                    const void *key,
                    int key_len,
                    const unsigned char *data,
                    int data_len,
                    unsigned char *hmac_out)
{
   crypto->hmac (crypto, key, key_len, data, data_len, hmac_out);
}

bool
mongoc_crypto_hash (mongoc_crypto_t *crypto, const unsigned char *input, const size_t input_len, unsigned char *output)
{
   return crypto->hash (crypto, input, input_len, output);
}
#endif

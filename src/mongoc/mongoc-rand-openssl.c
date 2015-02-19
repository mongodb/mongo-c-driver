/*
 * Copyright 2014 MongoDB, Inc.
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

#include "mongoc-config.h"

#ifdef MONGOC_ENABLE_SSL
#ifdef MONGOC_OPENSSL

#include "mongoc-rand-openssl.h"
#include "mongoc-rand-openssl-private.h"

#include "mongoc.h"

#include <openssl/rand.h>

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_rand_openssl_bytes --
 *
 *       Puts @num cryptographically secure bytes into @buf.
 *
 * Returns:
 *       1 on success, 0 otherwise, with error in ERR_get_error.
 *
 *-------------------------------------------------------------------------
 */

int _mongoc_rand_openssl_bytes(uint8_t * buf, int num) {
    return RAND_bytes(buf, num);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_pseudo_rand_bytes --
 *
 *       Puts @num cryptographically secure bytes into @buf.
 *
 * Returns:
 *       1 on success, 0 otherwise, with error in ERR_get_error.
 *
 *-------------------------------------------------------------------------
 */

int _mongoc_pseudo_rand_openssl_bytes(uint8_t * buf, int num) {
    return RAND_pseudo_bytes(buf, num);
}

void mongoc_rand_openssl_seed(const void* buf, int num) {
    RAND_seed(buf, num);
}

void mongoc_rand_openssl_add(const void* buf, int num, double entropy) {
    RAND_add(buf, num, entropy);
}

int mongoc_rand_openssl_status(void) {
    return RAND_status();
}

#endif /* MONGOC_OPENSSL */
#endif /* MONGOC_ENABLE_SSL */

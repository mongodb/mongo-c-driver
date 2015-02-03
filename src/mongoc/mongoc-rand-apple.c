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
#ifdef MONGOC_APPLE_NATIVE_TLS

#include "mongoc-rand-apple.h"
#include "mongoc-rand-apple-private.h"

#include "mongoc.h"

#include <Security/Security.h>

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_rand_apple_bytes --
 *
 *       Uses SecureTransport's default random number generator to fill
 *       @buf with @num cryptographically secure random bytes.
 *
 * Returns:
 *       1 on success, 0 on failure, with error in errno system variable.
 *
 *-------------------------------------------------------------------------
 */

int _mongoc_rand_apple_bytes(uint8_t * buf, int num) {
   if (0 == SecRandomCopyBytes(kSecRandomDefault, num, buf)) {
      return 1;
   }
   return 0;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_pseudo_rand_apple_bytes --
 *
 *       With SecureTransport, behaves like _mongoc_rand_bytes.
 *
 * Returns:
 *       1 on success, 0 on failure, with error in errno system variable.
 *
 *-------------------------------------------------------------------------
 */

int _mongoc_pseudo_rand_apple_bytes(uint8_t * buf, int num) {
   return _mongoc_rand_apple_bytes(buf, num);
}

void mongoc_rand_apple_seed(const void* buf, int num) {
    /* n/a */
    // TODO why is this n/a?
}

void mongoc_rand_apple_add(const void* buf, int num, double entropy) {
    /* n/a */
    // TODO why is this n/a?
}

int mongoc_rand_apple_status(void) {
    return 1;
}

#endif /* MONGOC_APPLE_NATIVE_TLS */
#endif /* MONGOC_ENABLE_SSL */

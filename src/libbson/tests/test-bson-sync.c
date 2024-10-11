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


#include <bson/bson.h>
#include <common-macros-private.h> // BEGIN_IGNORE_DEPRECATIONS

#include "TestSuite.h"

static void
test_bson_sync_synchronize (void)
{
   BEGIN_IGNORE_DEPRECATIONS

   // This doesn't test for correct functionality, only that it exists and can be called
   bson_sync_synchronize();

   END_IGNORE_DEPRECATIONS
}

void
test_bson_sync_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/sync/synchronize", test_bson_sync_synchronize);
}
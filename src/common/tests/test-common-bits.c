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

#include <common-bits-private.h>

#include <TestSuite.h>

static void
test_mcommon_next_power_of_two_u32(void)
{
   BSON_ASSERT(mcommon_next_power_of_two_u32(0u) == 1u);
   BSON_ASSERT(mcommon_next_power_of_two_u32(1u) == 1u);
   BSON_ASSERT(mcommon_next_power_of_two_u32(3u) == 4u);
   BSON_ASSERT(mcommon_next_power_of_two_u32(UINT32_C(0x80000000)) == UINT32_C(0x80000000));

   // Test saturation:
   BSON_ASSERT(mcommon_next_power_of_two_u32(UINT32_C(0x80000001)) == UINT32_MAX);
   BSON_ASSERT(mcommon_next_power_of_two_u32(UINT32_MAX) == UINT32_MAX);
}

static void
test_mcommon_next_power_of_two_size_t(void)
{
   const size_t max_two_power = (SIZE_MAX >> 1u) + 1u;

   BSON_ASSERT(mcommon_next_power_of_two_size_t(0u) == 1u);
   BSON_ASSERT(mcommon_next_power_of_two_size_t(1u) == 1u);
   BSON_ASSERT(mcommon_next_power_of_two_size_t(3u) == 4u);
   BSON_ASSERT(mcommon_next_power_of_two_size_t(max_two_power) == max_two_power);

   // Test saturation:
   BSON_ASSERT(mcommon_next_power_of_two_size_t(max_two_power + 1u) == SIZE_MAX);
   BSON_ASSERT(mcommon_next_power_of_two_size_t(SIZE_MAX - 1u) == SIZE_MAX);
   BSON_ASSERT(mcommon_next_power_of_two_size_t(SIZE_MAX) == SIZE_MAX);
}

void
test_mcommon_bits_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/mcommon/bits/next_power_of_two/u32", test_mcommon_next_power_of_two_u32);
   TestSuite_Add(suite, "/mcommon/bits/next_power_of_two/size_t", test_mcommon_next_power_of_two_size_t);
}

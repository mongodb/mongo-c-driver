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

#include <mongoc/mongoc-punycode-private.h>

#include <mlib/str.h>

#include <TestSuite.h>

// Canonicalize `domain` and expect success with the result `expect`.
#define ASSERT_CANONICAL(domain, expect)                                                    \
   do {                                                                                     \
      mstr _dst = mstr_new(0);                                                              \
      ASSERT_WITH_MSG(mongoc_punycode_canonicalize_domain((domain), strlen(domain), &_dst), \
                      "expected \"%s\" to canonicalize",                                    \
                      (domain));                                                            \
      ASSERT_CMPSTR(_dst.data, (expect));                                                   \
      mstr_destroy(&_dst);                                                                  \
   } while (0)

// Canonicalize `domain` and expect failure.
#define ASSERT_NOT_CANONICAL(domain)                                                         \
   do {                                                                                      \
      mstr _dst = mstr_new(0);                                                               \
      ASSERT_WITH_MSG(!mongoc_punycode_canonicalize_domain((domain), strlen(domain), &_dst), \
                      "expected \"%s\" to be rejected",                                      \
                      (domain));                                                             \
      mstr_destroy(&_dst);                                                                   \
   } while (0)

static void
test_punycode_ascii(void)
{
   ASSERT_CANONICAL("foo.bar", "foo.bar");
   ASSERT_CANONICAL("Foo.bar", "foo.bar");
   ASSERT_CANONICAL("FOO.BAR.EXAMPLE.COM", "foo.bar.example.com");
   ASSERT_CANONICAL("localhost", "localhost");
   // An existing A-label is not re-encoded, but is lowercased.
   ASSERT_CANONICAL("XN--MNCHEN-3YA.example.net", "xn--mnchen-3ya.example.net");
}

static void
test_punycode_non_ascii(void)
{
   ASSERT_CANONICAL("f\xc3\xb6o.bar", "xn--fo-fka.bar");
   ASSERT_CANONICAL("cluster.m\xc3\xbcnchen.example.net", "cluster.xn--mnchen-3ya.example.net");
}

// ASCII letters in a label that also has non-ASCII characters are lowercased.
static void
test_punycode_non_ascii_uppercase(void)
{
   ASSERT_CANONICAL("F\xc3\xb6o.bar", "xn--fo-fka.bar");
   ASSERT_CANONICAL("F\xc3\xb6O.BAR", "xn--fo-fka.bar");
   ASSERT_CANONICAL("M\xc3\xbcnchen.Example.NET", "xn--mnchen-3ya.example.net");
   // Mixed labels: only the non-ASCII one is encoded, all are lowercased.
   ASSERT_CANONICAL("Cluster0.M\xc3\xbcnchen.Example.NET", "cluster0.xn--mnchen-3ya.example.net");
}

static void
test_punycode_trailing_dot(void)
{
   ASSERT_CANONICAL("foo.bar.", "foo.bar");
   ASSERT_CANONICAL("Foo.Bar.", "foo.bar");
   ASSERT_CANONICAL("m\xc3\xbcnchen.example.net.", "xn--mnchen-3ya.example.net");
}

static void
test_punycode_invalid(void)
{
   // Empty input and empty labels.
   ASSERT_NOT_CANONICAL("");
   ASSERT_NOT_CANONICAL(".");
   ASSERT_NOT_CANONICAL(".foo.bar");
   ASSERT_NOT_CANONICAL("foo..bar");
   ASSERT_NOT_CANONICAL("foo.bar..");
   // Not valid UTF-8 in a label that must be encoded.
   ASSERT_NOT_CANONICAL("f\xff"
                        "o.bar");
   ASSERT_NOT_CANONICAL("f\xc3.bar");
}

// A failed canonicalization does not modify the destination.
static void
test_punycode_failure_keeps_dst(void)
{
   mstr dst = mstr_new(0);
   ASSERT(mongoc_punycode_canonicalize_domain("keep.me", strlen("keep.me"), &dst));
   ASSERT(!mongoc_punycode_canonicalize_domain("foo..bar", strlen("foo..bar"), &dst));
   ASSERT_CMPSTR(dst.data, "keep.me");
   mstr_destroy(&dst);
}

void
test_punycode_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/Punycode/ascii", test_punycode_ascii);
   TestSuite_Add(suite, "/Punycode/non_ascii", test_punycode_non_ascii);
   TestSuite_Add(suite, "/Punycode/non_ascii_uppercase", test_punycode_non_ascii_uppercase);
   TestSuite_Add(suite, "/Punycode/trailing_dot", test_punycode_trailing_dot);
   TestSuite_Add(suite, "/Punycode/invalid", test_punycode_invalid);
   TestSuite_Add(suite, "/Punycode/failure_keeps_dst", test_punycode_failure_keeps_dst);
}

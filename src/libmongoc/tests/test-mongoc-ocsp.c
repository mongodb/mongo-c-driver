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

#include <mongoc/mongoc-config.h>

#ifdef MONGOC_ENABLE_SSL_OPENSSL
#include <mongoc/mongoc-openssl-private.h>
#endif

#include <TestSuite.h>

#if defined(MONGOC_ENABLE_OCSP_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10101000L

#include <openssl/x509v3.h>

/* Appends an id-ad-ocsp accessLocation with the given URI to `aia`. The URI is
 * not validated, so a deliberately malformed URI may be used. */
static void
add_ocsp_uri (AUTHORITY_INFO_ACCESS *aia, const char *uri)
{
   ACCESS_DESCRIPTION *const ad = ACCESS_DESCRIPTION_new ();
   BSON_ASSERT (ad);

   ASN1_OBJECT_free (ad->method);
   ad->method = OBJ_nid2obj (NID_ad_OCSP);

   ASN1_IA5STRING *const ia5 = ASN1_IA5STRING_new ();
   BSON_ASSERT (ia5);
   BSON_ASSERT (ASN1_STRING_set (ia5, uri, -1));
   GENERAL_NAME_set0_value (ad->location, GEN_URI, ia5);

   BSON_ASSERT (sk_ACCESS_DESCRIPTION_push (aia, ad));
}

/* Returns a certificate whose Authority Information Access extension lists
 * `uris` as OCSP responder URIs, in order. */
static X509 *
cert_with_ocsp_uris (const char *const *uris, size_t uris_len)
{
   X509 *const cert = X509_new ();
   BSON_ASSERT (cert);

   AUTHORITY_INFO_ACCESS *const aia = AUTHORITY_INFO_ACCESS_new ();
   BSON_ASSERT (aia);
   for (size_t i = 0; i < uris_len; i++) {
      add_ocsp_uri (aia, uris[i]);
   }

   BSON_ASSERT (X509_add1_ext_i2d (cert, NID_info_access, aia, 0 /* not critical */, 0 /* flags */));
   AUTHORITY_INFO_ACCESS_free (aia);

   return cert;
}

/* Regression test for CDRIVER-6409 */
static void
test_ocsp_responder_url_parse_errors (void)
{
   const char *const uris[] = {
      "http://localhost:1/",     /* Parses; request is allocated, then freed. */
      "http://localhost:99999/", /* Port out of range: fails OCSP_parse_url
                                  * before the request is reallocated. */
   };

   X509 *const cert = cert_with_ocsp_uris (uris, sizeof (uris) / sizeof (uris[0]));
   mongoc_ssl_opt_t ssl_opts = {0};
   int ocsp_uri_count = 0;

   /* A NULL cert ID makes the first URI fail after the OCSP_REQUEST is
    * allocated, without any HTTP traffic. */
   OCSP_RESPONSE *const resp = _mongoc_contact_ocsp_responder (NULL /* id */, cert, &ssl_opts, &ocsp_uri_count);

   ASSERT_CMPINT (ocsp_uri_count, ==, 2);
   BSON_ASSERT (!resp);

   X509_free (cert);
}

void
test_ocsp_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/OCSP/responder/url_parse_errors", test_ocsp_responder_url_parse_errors);
}

#else
/* ensure the translation unit is not empty */
extern int no_mongoc_ocsp_responder;
#endif /* MONGOC_ENABLE_OCSP_OPENSSL */

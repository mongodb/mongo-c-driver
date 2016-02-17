#include <mongoc.h>

#include "TestSuite.h"

#define TRUST_DIR "tests/trust_dir"
#define VERIFY_DIR TRUST_DIR "/verify"
#define CRLFILE TRUST_DIR "/crl/root.crl.pem"
#define CAFILE TRUST_DIR "/verify/mongo_root.pem"
#define PEMFILE_PASS TRUST_DIR "/keys/pass.mongodb.com.pem"
#define PEMFILE_ALT TRUST_DIR "/keys/alt.mongodb.com.pem"
#define PEMFILE_LOCALHOST TRUST_DIR "/keys/127.0.0.1.pem"
#define PEMFILE_NOPASS TRUST_DIR "/keys/mongodb.com.pem"
#define PEMFILE_REV TRUST_DIR "/keys/rev.mongodb.com.pem"
#define PASSWORD "testpass"

static void
test_extract_subject (void)
{
   char *subject;

   subject = mongoc_ssl_extract_subject (BINARY_DIR"/../certificates/client.pem", NULL);
   ASSERT_CMPSTR (subject, "CN=client,OU=kerneluser,O=10Gen,L=New York City,ST=New York,C=US");
   bson_free (subject);
}


#ifndef MONGOC_ENABLE_OPENSSL
static void
test_extract_subject_extra (void)
{
   char *subject;

   subject = mongoc_ssl_extract_subject (PEMFILE_PASS, PASSWORD);
   ASSERT_CMPSTR (subject, "CN=pass.mongodb.com,OU=C Driver,O=MongoDB Inc.,L=New York,ST=NY,C=US");
   bson_free (subject);

   subject = mongoc_ssl_extract_subject (PEMFILE_ALT, NULL);
   ASSERT_CMPSTR (subject, "CN=alt.mongodb.com,OU=C Driver,O=MongoDB Inc.,L=New York,ST=NY,C=US");
   bson_free (subject);

   subject = mongoc_ssl_extract_subject (PEMFILE_LOCALHOST, NULL);
   ASSERT_CMPSTR (subject, "CN=127.0.0.1,OU=C Driver,O=MongoDB Inc.,L=New York,ST=NY,C=US");
   bson_free (subject);

   subject = mongoc_ssl_extract_subject (PEMFILE_NOPASS, NULL);
   ASSERT_CMPSTR (subject, "CN=mongodb.com,OU=C Driver,O=MongoDB Inc.,L=New York,ST=NY,C=US");
   bson_free (subject);

   subject = mongoc_ssl_extract_subject (PEMFILE_REV, NULL);
   ASSERT_CMPSTR (subject, "CN=rev.mongodb.com,OU=C Driver,O=MongoDB Inc.,L=New York,ST=NY,C=US");
   bson_free (subject);
}
#endif


void
test_x509_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/SSL/extract_subject", test_extract_subject);
#ifndef MONGOC_ENABLE_OPENSSL
   TestSuite_Add (suite, "/SSL/extract_subject/extra", test_extract_subject_extra);
#endif
}

#include <mongoc.h>
#include "mongoc-ssl-private.h"

#include "TestSuite.h"

#ifdef MONGOC_ENABLE_OPENSSL
static void
test_extract_subject (void)
{
   char *subject;

   subject = mongoc_ssl_extract_subject (CERT_SERVER, NULL);
   ASSERT_CMPSTR (subject, "C=IS,ST=Reykjavik,L=Reykjavik,O=MongoDB,OU=Drivers,CN=server");
   bson_free (subject);

   subject = mongoc_ssl_extract_subject (CERT_CLIENT, NULL);
   ASSERT_CMPSTR (subject, "C=IS,ST=Kopavogur,L=Kopavogur,O=MongoDB,OU=Drivers,CN=client");
   bson_free (subject);
}
#endif


void
test_x509_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_OPENSSL
   TestSuite_Add (suite, "/X509/extract_subject", test_extract_subject);
#endif
}

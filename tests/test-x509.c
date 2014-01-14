#include <mongoc.h>
#include <mongoc-ssl-private.h>

#include "mongoc-tests.h"


static void
test_extract_subject (void)
{
   char *subject;

   subject = _mongoc_ssl_extract_subject ("tests/certificates/client.pem");
   assert (0 == strcmp (subject, "CN=client,OU=kerneluser,O=10Gen,L=New York City,ST=New York,C=U"));
   bson_free (subject);
}


int
main (int argc,
      char *argv[])
{
   mongoc_init ();

   run_test ("/SSL/extract_subject", test_extract_subject);

   return 0;
}

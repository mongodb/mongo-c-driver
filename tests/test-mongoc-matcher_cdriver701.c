


#include "mongoc-tests.h"

#include <bcon.h>
#include <mongoc.h>
#include <mongoc-matcher-private.h>
#include <bson-types.h>
#include <string.h>
#include "TestSuite.h"





static void
subdoc_test (void)
{
   //this should be true
   bson_t *doc;
   bson_t *spec1;
   bson_t *spec2;
   bson_error_t error;
   mongoc_matcher_t *matcher1;
   mongoc_matcher_t *matcher2;

   bool r;
   doc  = BCON_NEW("main_doc", "{", "sub_doc", "[","item1", "item2", "item3", "]", "}");

   spec1 = BCON_NEW("main_doc.sub_doc", "item2");
   spec2 = BCON_NEW("main_doc", "{", "sub_doc", "[","item1", "]", "}");


   matcher1 = mongoc_matcher_new (spec1, &error);
   BSON_ASSERT (matcher1);
   r = mongoc_matcher_match (matcher1, doc);
   BSON_ASSERT (r);

   matcher2 = mongoc_matcher_new (spec2, &error);
   BSON_ASSERT (matcher2);
   r = mongoc_matcher_match (matcher2, doc);
   BSON_ASSERT (!r);


   bson_destroy (doc);
   bson_destroy (spec1);
   mongoc_matcher_destroy (matcher1);
   bson_destroy (spec2);
   mongoc_matcher_destroy (matcher2);

}


int
main (int   argc,
      char *argv[])
{
   subdoc_test();
   return 0;

}
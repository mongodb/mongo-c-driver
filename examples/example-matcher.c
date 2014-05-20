#include <bcon.h>
#include <mongoc.h>
#include <stdio.h>

static void
log_query (const bson_t *doc,
           const bson_t *query)
{
   char *str1;
   char *str2;

   str1 = bson_as_json (doc, NULL);
   str2 = bson_as_json (query, NULL);

   printf ("Matching %s against %s\n", str2, str1);

   bson_free (str1);
   bson_free (str2);
}

static void
example (void)
{
   mongoc_matcher_t *matcher;
   bson_error_t error;
   bson_t *query;
   bson_t *doc;

   doc = BCON_NEW ("hello", "[", "{", "foo", BCON_UTF8 ("bar"), "}", "]");
   query = BCON_NEW ("hello.0.foo", BCON_UTF8 ("bar"));

   log_query (doc, query);

   matcher = mongoc_matcher_new (query, &error);
   if (!matcher) {
      fprintf (stderr, "Error: %s\n", error.message);
      bson_destroy (query);
      bson_destroy (doc);
      return;
   }

   if (mongoc_matcher_match (matcher, doc)) {
      printf ("  Document matched!\n");
   }

   bson_destroy (query);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);
}

int
main (int argc,
      char *argv[])
{
   mongoc_init ();
   example ();
   mongoc_cleanup ();

   return 0;
}

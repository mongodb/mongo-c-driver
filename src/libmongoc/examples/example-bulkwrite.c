// example-bulkwrite shows use of `mongoc_client_bulkwrite`.

#include <mongoc/mongoc.h>

#define HANDLE_ERROR(...)            \
   if (1) {                          \
      fprintf (stderr, __VA_ARGS__); \
      fprintf (stderr, "\n");        \
      goto fail;                     \
   } else                            \
      (void) 0

int
main (int argc, char *argv[])
{
   bool ok = false;

   mongoc_init ();

   bson_error_t error;
   mongoc_client_t *client = mongoc_client_new ("mongodb://localhost:27017");
   mongoc_bulkwriteoptions_t *bwo = mongoc_bulkwriteoptions_new ();
   mongoc_bulkwriteoptions_set_verboseresults (bwo, true);
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client, bwo);

   // Insert a document to `db.coll1`
   {
      bson_t *doc = BCON_NEW ("foo", "1");
      if (!mongoc_client_bulkwrite_append_insertone (bw, "db.coll1", -1, doc, NULL, &error)) {
         HANDLE_ERROR ("error appending insert one: %s", error.message);
      }
      bson_destroy (doc);
   }
   // Insert a document to `db.coll2`
   {
      bson_t *doc = BCON_NEW ("foo", "2");
      if (!mongoc_client_bulkwrite_append_insertone (bw, "db.coll2", -1, doc, NULL, &error)) {
         HANDLE_ERROR ("error appending insert one: %s", error.message);
      }
      bson_destroy (doc);
   }

   mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw);

   printf ("insert count: %" PRId64 "\n", mongoc_bulkwriteresult_insertedcount (bwr.res));

   // Print verbose results.
   {
      const bson_t *vr = mongoc_bulkwriteresult_verboseresults (bwr.res);
      BSON_ASSERT (vr);
      char *vr_str = bson_as_relaxed_extended_json (vr, NULL);
      printf ("verbose results: %s\n", vr_str);
      bson_free (vr_str);
   }

   // Print exception.
   if (bwr.exc) {
      const bson_t *error_doc;
      mongoc_bulkwriteexception_error (bwr.exc, &error, &error_doc);
      if (mongoc_error_has_label (error_doc, "RetryableWriteError")) {
         printf ("error has label: RetryableWriteError\n");
      }
      printf ("error: %s\n", error.message);
      char *error_doc_str = bson_as_relaxed_extended_json (error_doc, NULL);
      printf ("exception: %s\n", error_doc_str);
      bson_free (error_doc_str);
   }

   mongoc_bulkwriteresult_destroy (bwr.res);
   mongoc_bulkwriteexception_destroy (bwr.exc);
   mongoc_bulkwrite_destroy (bw);

   ok = true;
fail:
   mongoc_client_destroy (client);
   mongoc_bulkwriteoptions_destroy (bwo);
   mongoc_cleanup ();
   return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

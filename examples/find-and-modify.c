#include <mongoc.h>
#include <stdio.h>


/*
 *--------------------------------------------------------------------------
 *
 * find_and_modify --
 *
 *       Find a document in @collection matching @query and update it with
 *       the update document @update.
 *
 *       If @reply is not NULL, then the result document will be placed
 *       in reply and should be released with bson_destroy().
 *
 *       If @remove is true, then the matching documents will be removed.
 *
 *       If @fields is not NULL, it will be used to select the desired
 *       resulting fields.
 *
 *       If @_new is true, then the new version of the document is returned
 *       instead of the old document.
 *
 *       See http://docs.mongodb.org/manual/reference/command/findAndModify/
 *       for more information.
 *
 * Returns:
 *       1 on success, otherwise 0.
 *
 * Side effects:
 *       @reply will be set if 1 is returned.
 *
 *--------------------------------------------------------------------------
 */

static bool
find_and_modify (mongoc_collection_t *collection, /* IN */
                 const bson_t *query,             /* IN */
                 const bson_t *sort,              /* IN */
                 const bson_t *update,            /* IN */
                 const bson_t *fields,            /* IN */
                 bool remove,              /* IN */
                 bool upsert,              /* IN */
                 bool _new,                /* IN */
                 bson_t *reply,                   /* OUT */
                 bson_error_t *error)             /* OUT */
{
   const char *name;
   bool ret;
   bson_t command;

   BSON_ASSERT (collection);
   BSON_ASSERT (query);
   BSON_ASSERT (update || remove);

   name = mongoc_collection_get_name (collection);

   /*
    * Build our findAndModify command.
    */
   bson_init (&command);
   bson_append_utf8 (&command, "findAndModify", -1, name, -1);
   bson_append_document (&command, "query", -1, query);
   if (sort)
      bson_append_document (&command, "sort", -1, sort);
   if (update)
      bson_append_document (&command, "update", -1, update);
   if (fields)
      bson_append_document (&command, "fields", -1, fields);
   if (remove)
      bson_append_bool (&command, "remove", -1, remove);
   if (upsert)
      bson_append_bool (&command, "upsert", -1, upsert);
   if (_new)
      bson_append_bool (&command, "new", -1, _new);

   /*
    * Submit the command to MongoDB server.
    */
   ret = mongoc_collection_command_simple (collection, &command, NULL, reply, error);

   /*
    * Cleanup.
    */
   bson_destroy (&command);

   return ret;
}


int
main (int   argc,
      char *argv[])
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   bson_error_t error;
   bson_t query;
   bson_t update;
   bson_t child;
   bson_t reply;
   char *str;

   mongoc_init ();

   client = mongoc_client_new ("mongodb://127.0.0.1:27017/");
   collection = mongoc_client_get_collection (client, "test", "test");

   /*
    * Build our query, {"cmpxchg": 1}
    */
   bson_init (&query);
   bson_append_int32 (&query, "cmpxchg", -1, 1);

   /*
    * Build our update. {"$set": {"cmpxchg": 2}}
    */
   bson_init (&update);
   bson_append_document_begin (&update, "$set", -1, &child);
   bson_append_int32 (&child, "cmpxchg", -1, 2);
   bson_append_document_end (&update, &child);

   /*
    * Submit the findAndModify.
    */
   if (!find_and_modify (collection, &query, NULL, &update, NULL, false, false, true, &reply, &error)) {
      fprintf (stderr, "find_and_modify() failure: %s\n", error.message);
      return 1;
   }

   /*
    * Print the result as JSON.
    */
   str = bson_as_json (&reply, NULL);
   printf ("%s\n", str);
   bson_free (str);

   /*
    * Cleanup.
    */
   bson_destroy (&query);
   bson_destroy (&update);
   bson_destroy (&reply);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);

   return 0;
}

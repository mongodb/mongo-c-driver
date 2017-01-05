:man_page: mongoc_find_and_modify_opts_t

mongoc_find_and_modify_opts_t
=============================

find_and_modify abstraction

Synopsis
--------

``mongoc_find_and_modify_opts_t`` is a builder interface to construct a `find_and_modify <https://docs.mongodb.org/manual/reference/command/findAndModify/>`_ command.

It was created to be able to accommodate new arguments to the MongoDB find_and_modify command.

.. tip::

  New in mongoc 1.3.0

.. tip::

  As of MongoDB 3.2, the :symbol:`mongoc_write_concern_t <mongoc_write_concern_t>` specified on the :symbol:`mongoc_collection_t <mongoc_collection_t>` will be used, if any.

Example
-------

.. code-block:: none

        
  void
  fam_flags (mongoc_collection_t *collection)
  {
     mongoc_find_and_modify_opts_t *opts;
     bson_t reply;
     bson_error_t error;
     bson_t query = BSON_INITIALIZER;
     bson_t *update;
     bool success;


     /* Find Zlatan Ibrahimovic, the striker */
     BSON_APPEND_UTF8 (&query, "firstname", "Zlatan");
     BSON_APPEND_UTF8 (&query, "lastname", "Ibrahimovic");
     BSON_APPEND_UTF8 (&query, "profession", "Football player");
     BSON_APPEND_INT32 (&query, "age", 34);
     BSON_APPEND_INT32 (
        &query, "goals", (16 + 35 + 23 + 57 + 16 + 14 + 28 + 84) + (1 + 6 + 62));

     /* Add his football position */
     update = BCON_NEW ("$set", "{", "position", BCON_UTF8 ("striker"), "}");

     opts = mongoc_find_and_modify_opts_new ();

     mongoc_find_and_modify_opts_set_update (opts, update);

     /* Create the document if it didn't exist, and return the updated document */
     mongoc_find_and_modify_opts_set_flags (
        opts, MONGOC_FIND_AND_MODIFY_UPSERT | MONGOC_FIND_AND_MODIFY_RETURN_NEW);

     success = mongoc_collection_find_and_modify_with_opts (
        collection, &query, opts, &reply, &error);

     if (success) {
        char *str;

        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
     } else {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
     }

     bson_destroy (&reply);
     bson_destroy (update);
     bson_destroy (&query);
     mongoc_find_and_modify_opts_destroy (opts);
  }

  void
  fam_bypass (mongoc_collection_t *collection)
  {
     mongoc_find_and_modify_opts_t *opts;
     bson_t reply;
     bson_t *update;
     bson_error_t error;
     bson_t query = BSON_INITIALIZER;
     bool success;


     /* Find Zlatan Ibrahimovic, the striker */
     BSON_APPEND_UTF8 (&query, "firstname", "Zlatan");
     BSON_APPEND_UTF8 (&query, "lastname", "Ibrahimovic");
     BSON_APPEND_UTF8 (&query, "profession", "Football player");

     /* Bump his age */
     update = BCON_NEW ("$inc", "{", "age", BCON_INT32 (1), "}");

     opts = mongoc_find_and_modify_opts_new ();
     mongoc_find_and_modify_opts_set_update (opts, update);
     /* He can still play, even though he is pretty old. */
     mongoc_find_and_modify_opts_set_bypass_document_validation (opts, true);

     success = mongoc_collection_find_and_modify_with_opts (
        collection, &query, opts, &reply, &error);

     if (success) {
        char *str;

        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
     } else {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
     }

     bson_destroy (&reply);
     bson_destroy (update);
     bson_destroy (&query);
     mongoc_find_and_modify_opts_destroy (opts);
  }

  void
  fam_update (mongoc_collection_t *collection)
  {
     mongoc_find_and_modify_opts_t *opts;
     bson_t *update;
     bson_t reply;
     bson_error_t error;
     bson_t query = BSON_INITIALIZER;
     bool success;


     /* Find Zlatan Ibrahimovic */
     BSON_APPEND_UTF8 (&query, "firstname", "Zlatan");
     BSON_APPEND_UTF8 (&query, "lastname", "Ibrahimovic");

     /* Make him a book author */
     update = BCON_NEW ("$set", "{", "author", BCON_BOOL (true), "}");

     opts = mongoc_find_and_modify_opts_new ();
     /* Note that the document returned is the _previous_ version of the document
      * To fetch the modified new version, use
      * mongoc_find_and_modify_opts_set_flags (opts,
      * MONGOC_FIND_AND_MODIFY_RETURN_NEW);
      */
     mongoc_find_and_modify_opts_set_update (opts, update);

     success = mongoc_collection_find_and_modify_with_opts (
        collection, &query, opts, &reply, &error);

     if (success) {
        char *str;

        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
     } else {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
     }

     bson_destroy (&reply);
     bson_destroy (update);
     bson_destroy (&query);
     mongoc_find_and_modify_opts_destroy (opts);
  }

  void
  fam_fields (mongoc_collection_t *collection)
  {
     mongoc_find_and_modify_opts_t *opts;
     bson_t fields = BSON_INITIALIZER;
     bson_t *update;
     bson_t reply;
     bson_error_t error;
     bson_t query = BSON_INITIALIZER;
     bool success;


     /* Find Zlatan Ibrahimovic */
     BSON_APPEND_UTF8 (&query, "lastname", "Ibrahimovic");
     BSON_APPEND_UTF8 (&query, "firstname", "Zlatan");

     /* Return his goal tally */
     BSON_APPEND_INT32 (&fields, "goals", 1);

     /* Bump his goal tally */
     update = BCON_NEW ("$inc", "{", "goals", BCON_INT32 (1), "}");

     opts = mongoc_find_and_modify_opts_new ();
     mongoc_find_and_modify_opts_set_update (opts, update);
     mongoc_find_and_modify_opts_set_fields (opts, &fields);
     /* Return the new tally */
     mongoc_find_and_modify_opts_set_flags (opts,
                                            MONGOC_FIND_AND_MODIFY_RETURN_NEW);

     success = mongoc_collection_find_and_modify_with_opts (
        collection, &query, opts, &reply, &error);

     if (success) {
        char *str;

        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
     } else {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
     }

     bson_destroy (&reply);
     bson_destroy (update);
     bson_destroy (&fields);
     bson_destroy (&query);
     mongoc_find_and_modify_opts_destroy (opts);
  }

  void
  fam_sort (mongoc_collection_t *collection)
  {
     mongoc_find_and_modify_opts_t *opts;
     bson_t *update;
     bson_t sort = BSON_INITIALIZER;
     bson_t reply;
     bson_error_t error;
     bson_t query = BSON_INITIALIZER;
     bool success;


     /* Find all users with the lastname Ibrahimovic */
     BSON_APPEND_UTF8 (&query, "lastname", "Ibrahimovic");

     /* Sort by age (descending) */
     BSON_APPEND_INT32 (&sort, "age", -1);

     /* Bump his goal tally */
     update = BCON_NEW ("$set", "{", "oldest", BCON_BOOL (true), "}");

     opts = mongoc_find_and_modify_opts_new ();
     mongoc_find_and_modify_opts_set_update (opts, update);
     mongoc_find_and_modify_opts_set_sort (opts, &sort);

     success = mongoc_collection_find_and_modify_with_opts (
        collection, &query, opts, &reply, &error);

     if (success) {
        char *str;

        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
     } else {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
     }

     bson_destroy (&reply);
     bson_destroy (update);
     bson_destroy (&sort);
     bson_destroy (&query);
     mongoc_find_and_modify_opts_destroy (opts);
  }

  void
  fam_opts (mongoc_collection_t *collection)
  {
     mongoc_find_and_modify_opts_t *opts;
     bson_t reply;
     bson_t *update;
     bson_error_t error;
     bson_t query = BSON_INITIALIZER;
     bson_t extra = BSON_INITIALIZER;
     bool success;


     /* Find Zlatan Ibrahimovic, the striker */
     BSON_APPEND_UTF8 (&query, "firstname", "Zlatan");
     BSON_APPEND_UTF8 (&query, "lastname", "Ibrahimovic");
     BSON_APPEND_UTF8 (&query, "profession", "Football player");

     /* Bump his age */
     update = BCON_NEW ("$inc", "{", "age", BCON_INT32 (1), "}");

     opts = mongoc_find_and_modify_opts_new ();
     mongoc_find_and_modify_opts_set_update (opts, update);

     /* Abort if the operation takes too long. */
     mongoc_find_and_modify_opts_set_max_time_ms (opts, 100);

     /* Some future findAndModify option the driver doesn't support conveniently
      */
     BSON_APPEND_INT32 (&extra, "futureOption", 42);
     mongoc_find_and_modify_opts_append (opts, &extra);

     success = mongoc_collection_find_and_modify_with_opts (
        collection, &query, opts, &reply, &error);

     if (success) {
        char *str;

        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
     } else {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
     }

     bson_destroy (&reply);
     bson_destroy (&extra);
     bson_destroy (update);
     bson_destroy (&query);
     mongoc_find_and_modify_opts_destroy (opts);
  }

  #include <bcon.h>
  #include <mongoc.h>

  #include "flags.c"
  #include "bypass.c"
  #include "update.c"
  #include "fields.c"
  #include "opts.c"
  #include "sort.c"

  int
  main (void)
  {
     mongoc_collection_t *collection;
     mongoc_database_t *database;
     mongoc_client_t *client;
     bson_error_t error;
     bson_t *options;

     mongoc_init ();
     client = mongoc_client_new (
        "mongodb://localhost:27017/admin?appname=find-and-modify-opts-example");
     mongoc_client_set_error_api (client, 2);
     database = mongoc_client_get_database (client, "databaseName");

     options = BCON_NEW ("validator",
                         "{",
                         "age",
                         "{",
                         "$lte",
                         BCON_INT32 (34),
                         "}",
                         "}",
                         "validationAction",
                         BCON_UTF8 ("error"),
                         "validationLevel",
                         BCON_UTF8 ("moderate"));

     collection = mongoc_database_create_collection (
        database, "collectionName", options, &error);
     if (!collection) {
        fprintf (
           stderr, "Got error: \"%s\" on line %d\n", error.message, __LINE__);
        return 1;
     }

     fam_flags (collection);
     fam_bypass (collection);
     fam_update (collection);
     fam_fields (collection);
     fam_opts (collection);
     fam_sort (collection);

     mongoc_collection_drop (collection, NULL);
     bson_destroy (options);
     mongoc_database_destroy (database);
     mongoc_collection_destroy (collection);
     mongoc_client_destroy (client);

     mongoc_cleanup ();
     return 0;
  }

Outputs:

.. code-block:: none

          {
      "lastErrorObject": {
          "updatedExisting": false,
          "n": 1,
          "upserted": {
              "$oid": "56562a99d13e6d86239c7b00"
          }
      },
      "value": {
          "_id": {
              "$oid": "56562a99d13e6d86239c7b00"
          },
          "age": 34,
          "firstname": "Zlatan",
          "goals": 342,
          "lastname": "Ibrahimovic",
          "profession": "Football player",
          "position": "striker"
      },
      "ok": 1
  }
  {
      "lastErrorObject": {
          "updatedExisting": true,
          "n": 1
      },
      "value": {
          "_id": {
              "$oid": "56562a99d13e6d86239c7b00"
          },
          "age": 34,
          "firstname": "Zlatan",
          "goals": 342,
          "lastname": "Ibrahimovic",
          "profession": "Football player",
          "position": "striker"
      },
      "ok": 1
  }
  {
      "lastErrorObject": {
          "updatedExisting": true,
          "n": 1
      },
      "value": {
          "_id": {
              "$oid": "56562a99d13e6d86239c7b00"
          },
          "age": 35,
          "firstname": "Zlatan",
          "goals": 342,
          "lastname": "Ibrahimovic",
          "profession": "Football player",
          "position": "striker"
      },
      "ok": 1
  }
  {
      "lastErrorObject": {
          "updatedExisting": true,
          "n": 1
      },
      "value": {
          "_id": {
              "$oid": "56562a99d13e6d86239c7b00"
          },
          "goals": 343
      },
      "ok": 1
  }
  {
      "lastErrorObject": {
          "updatedExisting": true,
          "n": 1
      },
      "value": {
          "_id": {
              "$oid": "56562a99d13e6d86239c7b00"
          },
          "age": 35,
          "firstname": "Zlatan",
          "goals": 343,
          "lastname": "Ibrahimovic",
          "profession": "Football player",
          "position": "striker",
          "author": true
      },
      "ok": 1
  }
        

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_find_and_modify_opts_append
    mongoc_find_and_modify_opts_destroy
    mongoc_find_and_modify_opts_get_bypass_document_validation
    mongoc_find_and_modify_opts_get_fields
    mongoc_find_and_modify_opts_get_flags
    mongoc_find_and_modify_opts_get_max_time_ms
    mongoc_find_and_modify_opts_get_sort
    mongoc_find_and_modify_opts_get_update
    mongoc_find_and_modify_opts_new
    mongoc_find_and_modify_opts_set_bypass_document_validation
    mongoc_find_and_modify_opts_set_fields
    mongoc_find_and_modify_opts_set_flags
    mongoc_find_and_modify_opts_set_max_time_ms
    mongoc_find_and_modify_opts_set_sort
    mongoc_find_and_modify_opts_set_update


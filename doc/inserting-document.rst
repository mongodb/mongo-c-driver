:man_page: mongoc_inserting_document

Inserting a Document
====================

To insert documents into a collection, first obtain a handle to a :symbol:`mongoc_collection_t <mongoc_collection_t>` via a :symbol:`mongoc_client_t <mongoc_client_t>`. Then, use :symbol:`mongoc_collection_insert() <mongoc_collection_insert>` to add BSON documents to the collection. This example inserts into the database "mydb" and collection "mycoll".

When finished, ensure that allocated structures are freed by using their respective destroy functions.

.. code-block:: none

  #include <bson.h>
  #include <mongoc.h>
  #include <stdio.h>

  int
  main (int   argc,
        char *argv[])
  {
      mongoc_client_t *client;
      mongoc_collection_t *collection;
      mongoc_cursor_t *cursor;
      bson_error_t error;
      bson_oid_t oid;
      bson_t *doc;

      mongoc_init ();

      client = mongoc_client_new ("mongodb://localhost:27017/");
      collection = mongoc_client_get_collection (client, "mydb", "mycoll");

      doc = bson_new ();
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (doc, "_id", &oid);
      BSON_APPEND_UTF8 (doc, "hello", "world");

      if (!mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, NULL, &error)) {
          fprintf (stderr, "%s\n", error.message);
      }

      bson_destroy (doc);
      mongoc_collection_destroy (collection);
      mongoc_client_destroy (client);
      mongoc_cleanup ();

      return 0;
  }

Compile the code and run it:

.. code-block:: none

  $ gcc -o insert insert.c $(pkg-config --cflags --libs libmongoc-1.0)
  $ ./insert

On Windows:

.. code-block:: none

  C:\> cl.exe /IC:\mongo-c-driver\include\libbson-1.0 /IC:\mongo-c-driver\include\libmongoc-1.0 insert.c
  C:\> insert

To verify that the insert succeeded, connect with the MongoDB shell.

.. code-block:: none

  $ mongo
  MongoDB shell version: 3.0.6
  connecting to: test
  > use mydb
  switched to db mydb
  > db.mycoll.find()
  { "_id" : ObjectId("55ef43766cb5f36a3bae6ee4"), "hello" : "world" }
  > 


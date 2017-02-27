MongoDB C Driver
================

**A Cross Platform MongoDB Client Library for C**

The MongoDB C Driver, also known as "libmongoc", is the official client library for C applications, and provides a base for MongoDB drivers in higher-level languages.

The library is compatible with all major platforms. It depends on libbson to create and parse BSON data.

Download
--------

Latest release:

:download-link:`mongoc`

Example: Count documents in a collection
----------------------------------------

.. code-block:: c

   #include <mongoc.h>
   #include <bcon.h>
   #include <stdio.h>

   static void
   print_query_count (mongoc_collection_t *collection, bson_t *query)
   {
      bson_error_t error;
      int64_t count;

      count = mongoc_collection_count (
         collection, MONGOC_QUERY_NONE, query, 0, 0, NULL, &error);

      if (count < 0) {
         fprintf (stderr, "Count failed: %s\n", error.message);
      } else {
         printf ("%" PRId64 " documents counted.\n", count);
      }
   }

Documentation
-------------

`Installation <libmongoc/current/installing.html>`_

`Tutorial <libmongoc/current/tutorial.html>`_

`libmongoc reference <libmongoc/current/index.html>`_

`libbson reference <libbson/current/index.html>`_

How To Ask For Help
-------------------

For help using the driver: `MongoDB Users Mailing List <http://groups.google.com/group/mongodb-user>`_.

To file a bug or feature request: `MongoDB Jira Issue Tracker <https://jira.mongodb.org/browse/CDRIVER>`_.

Documentation for Older Versions
--------------------------------

`libmongoc releases <libmongoc/index.html>`_

`libbson releases <libbson/index.html>`_

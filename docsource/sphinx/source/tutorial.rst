MongoDB C Driver Tutorial
=========================


This document is an introduction to usage of the MongoDB database from a C program.

First, install Mongo.

Next, you may wish to take a look at the guide for a language independent look at how
to use MongoDB.&nbsp; Also, we suggest some basic familiarity with the MongoDB shell.

A working C program complete with examples from this tutorial can be found
`here <https://gist.github.com/920297>`_.

Connecting
----------

Let's make a tutorial.c file that connects to the database:

.. code-block:: c

    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include "bson.h"
    #include "mongo.h"

    int main() {
      mongo conn[1];
      status = mongo_connect( conn, "localhost", 27017 );

      if( status != MONGO_OK ) {
          switch ( conn->err ) {
            case MONGO_CONN_SUCCESS:    printf( "connection succeeded\n" ); break;
            case MONGO_CONN_BAD_ARG:    printf( "bad arguments\n" ); return 1;
            case MONGO_CONN_NO_SOCKET:  printf( "no socket\n" ); return 1;
            case MONGO_CONN_FAIL:       printf( "connection failed\n" ); return 1;
            case MONGO_CONN_NOT_MASTER: printf( "not master\n" ); return 1;
          }
      }

      mongo_destroy( conn );
      printf( "\nconnection closed\n" );

      return 0;
    }

Connecting to a replica set
---------------------------

The API for connecting to a replica set is slightly different. First you initialize
the connection object, specifying the replica set's name, then you add seed nodes,
and finally you connect. Here's an example:

.. code-block:: c

    #include "mongo.h"

    int main() {
      mongo conn[1];

      mongo_replset_add_seed( "10.4.3.22", 27017 );
      mongo_replset_add_seed( "10.4.3.32", 27017 );

      mongo_connect( conn );

      status = mongo_replset_connect( conn, "my-repl-set" );

      if( status != MONGO_OK ) {
          // Check conn->err for error code.
      }

      mongo_destroy( conn );

      return 0;
    }

You'd then proceed to check the status as we did in the previous example.

Building the sample program
---------------------------

If you are using gcc on Linux or OS X, you would compile with something like this, depending on location of your include files:

.. code-block:: bash

    $ gcc -Isrc --std=c99 /path/to/mongo-c-driver/src/*.c -I /path/to/mongo-c-driver/src/ tutorial.c -o tutorial
    $ ./tutorial
    connection succeeded
    connection closed

BSON
----

MongoDB database stores data in a format called *BSON*. BSON is a JSON-like binary object format.

To save data in the database we must create bson objects. We use ``bson_buffer`` to make ``bson``
objects, ``and bson_iterator`` to enumerate ``bson`` objects.

Let's now create a BSON "person" object which contains name and age. We might invoke:

.. code-block:: c

  bson b[1];

  bson_init( b )
  bson_append_string( b, "name", "Joe" );
  bson_append_int( b, "age", 33 );
  bson_finish( b );

  mongo_insert( conn, b );

  bson_destroy( b );

Use the ``bson_append_new_oid()`` function to add an object id to your object.
The server will add an object id to the ``_id`` field if it is not included explicitly.

.. code-block:: c

    bson b[1];

    bson_init( b );
    bson_append_new_oid( b, "_id" );
    bson_append_string( b, "name", "Joe" );
    bson_append_int( b, "age", 33 );
    bson_finish( b );

``bson_buffer_new_oid( ..., "_id" )`` should be at the beginning of the generated object.

When you are done using the object remember to use ``bson_destroy()`` to free up the memory allocated by the buffer.

.. code-block:: c

    bson_destroy( b )

Inserting a single document
---------------------------

Here's how we save our person object to the database's "people" collection:

.. code-block:: c

    mongo_insert( conn, "tutorial.people", b );

The first parameter to ``mongo_insert`` is the pointer to the ``mongo_connection``
object. The second parameter is the namespace, which include the database name, followed
by a dot followed by the collection name. Thus, ``tutorial`` is the database and ``people``
is the collection name. The third parameter is a pointer to the ``bson`` object that
we created before.

Inserting a batch of documents
------------------------------

We can do batch inserts as well:

.. code-block:: c

    static void tutorial_insert_batch( mongo_connection *conn ) {
      bson *p, **ps;
      char *names[4];
      int ages[] = { 29, 24, 24, 32 };
      int i, n = 4;
      names[0] = "Eliot"; names[1] = "Mike"; names[2] = "Mathias"; names[3] = "Richard";

      ps = ( bson ** )malloc( sizeof( bson * ) * n);

      for ( i = 0; i < n; i++ ) {
        p = ( bson * )malloc( sizeof( bson ) );
        bson_init( p );
        bson_append_new_oid( p_buf, "_id" );
        bson_append_string( p_buf, "name", names[i] );
        bson_append_int( p_buf, "age", ages[i] );
        bson_finish( p );
        ps[i] = p;
      }

      mongo_insert_batch( conn, "tutorial.persons", ps, n );

      for ( i = 0; i < n; i++ ) {
        bson_destroy( ps[i] );
        free( ps[i] );
      }
    }

Simple Queries
--------------

Let's now fetch all objects from the persons collection, and display them.

.. code-block:: c

    static void tutorial_empty_query( mongo_connection *conn) {
      mongo_cursor *cursor;
      bson empty[1];
      bson_empty( empty );

      cursor = mongo_find( conn, "tutorial.persons", empty, empty, 0, 0, 0 );
        while( mongo_cursor_next( cursor ) == MONGO_OK ) {
        bson_print( &cursor->current );
      }

      mongo_cursor_destroy( cursor );
      bson_destroy( empty );
    }

``empty`` is the empty BSON object \-\- we use it to represent what we
mean by ``{}`` in JSON: an empty query pattern (an empty query is a query for all objects).

We use ``bson_print()`` to print an abbreviated JSON string representation of the object.

``mongo_find()`` returns a ``mongo_cursor``, which must be destroyed after use.

Let's now write a function which prints out the name of all persons in the collection
whose age is a given value:

.. code-block:: c

    static void tutorial_simple_query( mongo_connection *conn ) {
      bson query[1];
      mongo_cursor *cursor;

      bson_init( query );
      bson_append_int( query_buf, "age", 24 );
      bson_from_buffer( query, query_buf );

      cursor = mongo_find( conn, "tutorial.persons", query, NULL, 0, 0, 0 );
      while( mongo_cursor_next( cursor ) == MONGO_OK ) {
        bson_iterator it[1];
        if ( bson_find( it, &cursor->current, "name" )) {
          printf( "name: %s\n", bson_iterator_string( it ) );
        }
      }

      bson_destroy( query );
      mongo_cursor_destroy( cursor );
    }

Our query above, written as JSON, is of the form

.. code-block:: javascript

    { age : 24 }

In the mongo shell (which uses javascript), we could invoke:

.. code-block:: javascript

    use tutorial;
    db.persons.find( { age : 24 } );

Complex Queries
---------------

Sometimes we want to do more then a simple query. We may want the results to
be sorted in a special way, or what the query to use a certain index.

Let's now make the results from previous query be sorted alphabetically by name.
To do this, we change the query statement from:

.. code-block:: c

    bson_init( query );
    bson_append_int( query, "age", 24 );
    bson_finish( query );

to:

.. code-block:: c

    bson_init( query );
      bson_append_start_object( query, "$query" );
        bson_append_int( query, "age", 24 );
      bson_append_finish_object( query );

      bson_append_start_object( query, "$orderby" );
        bson_append_int( query, "name", 1);
      bson_append_finish_object( query );
    bson_from_buffer( query, query );

Indexing
--------

Let's suppose we want to have an index on age so that our queries are fast. Here's
how we can create that index:

.. code-block:: c

    static void tutorial_index( mongo_connection *conn ) {
      bson key[1];

      bson_init( key );
      bson_append_int( key, "name", 1 );
      bson_finish( key );

      mongo_create_index( conn, "tutorial.persons", key, 0, NULL );

      bson_destroy( key );

      printf( "simple index created on \"name\"\n" );

      bson_init( key );
      bson_append_int( key, "age", 1 );
      bson_append_int( key, "name", 1 );
      bson_finish( key );

      mongo_create_index( conn, "tutorial.persons", key, 0, NULL );

      bson_destroy( key );

      printf( "compound index created on \"age\", \"name\"\n" );
    }


Updating documents
------------------

Use the ``mongo_update()`` function to perform a updates.
For example the following update in the MongoDB shell:

.. code-block:: javascript

    use tutorial
    db.persons.update( { name : 'Joe', age : 33 },
                       { $inc : { visits : 1 } } )

is equivalent to the following C function:

.. code-block:: c

    static void tutorial_update( mongo_connection *conn ) {
      bson cond[1], op[1];

      bson_init( cond );
      bson_append_string( cond, "name", "Joe");
      bson_append_int( cond, "age", 33);
      bson_finish( cond );

      bson_init( op );
      bson_append_start_object( op, "$inc" );
        bson_append_int( op, "visits", 1 );
      bson_append_finish_object( op );
      bson_finish( op );

      mongo_update(conn, "tutorial.persons", cond, op, 0);

      bson_destroy( cond );
      bson_destroy( op );
    }

Further Reading
---------------

This overview just touches on the basics of using Mongo from C.

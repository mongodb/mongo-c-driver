BSON
=============================

BSON (i.e., binary structured object notation) is the binary format used
by MongoDB to store data and express queries and commands. To work with
MongoDB is to trade in BSON objects. This document describes how to
create, read, destory BSON object from the MongoDB C Driver.

Libraries
---------

A brief note on libraries.

When you compile the driver, the BSON library is included in the
driver. This means that when you include ``mongo.h``, you have access
to all the functions declared in ``bson.h``.

If you want to use BSON independently, you don't need ``libmongoc``: when you compile
the driver, you'll also get shared and static libraries for ``libbson``. You
can link to this library and simple require ``bson.h``.

Using BSON objects
------------------

The pattern of BSON object usage is pretty simple. Here are the steps:

1. Initiate a new BSON object.
2. Construct the object using the bson_append_* methods.
3. Pass the object to bson_finish() to finalize it. The object is now ready to use.
4. When you're done with it, pass the object to bson_destroy() to free up any allocated
   memory.

To demonstrate, let's create a BSON object corresponding to the simple JSON object
``{count: 1001}``.

.. code: c

    bson b[1];

    bson_init( b );
    bson_append_int( b, "count", 1001 );
    bson_finish( b );

    // BSON object now ready for use

    bson_destroy( b );

That's all there is to creating a basic object.

Creating complex BSON objects
_____________________________

BSON objects can contain arrays as well as sub-objects. Here
we'll see how to create these by building the bson object
corresponding to the following JSON object:

.. code: javascript

    {
      name: "Kyle",

      colors: [ "red", "blue", "green" ],

      address: {
        city: "New York",
        zip: "10011-4567"
      }
    }

.. code: c

     bson b[1];

     bson_init( b );
     bson_append_string( b, "name", "Kyle" );

     bson_append_start_array( b, "colors" );
       bson_append_string( b, "0", "red" );
       bson_append_string( b, "1", "blue" );
       bson_append_string( b, "2", "green" );
     bson_append_finish_array( b );

     bson_append_start_object( b, "address" );
       bson_append_string( b, "city", "New York" );
       bson_append_string( b, "zip", "10011-4567" );
     bson_append_finish_object( b );

     if( bson_finish( b ) != BSON_OK )
         printf(" Error. ");

Notice that for the array, we have to manually set the index values
from "0" to *n*, where *n* is the number of elements in the array.

You'll notice that some knowledge of the BSON specification and
of the available types is necessary. For that, take a few minutes to
consult the 'official BSON specification <http://bsonspec.org>'_.

Error handling
--------------

The names of BSON object values, as well as all strings, must be
encoded as valid UTF-8. The BSON library will automatically check
the encoding of strings as you create BSON objects, and if the objects
are invalid, you'll be able to check for this condition. All of the
bson_append_* methods will return either BSON_OK for BSON_ERROR. You
can check in your code for the BSON_ERROR condition and then see the
exact nature of the error by examining the bson->err field. This bitfield
can contain any of the following values:

* BSON_VALID
* BSON_NOT_UTF8
* BSON_FIELD_HAS_DOT
* BSON_FIELD_INIT_DOLLAR
* BSON_ALREADY_FINISHED

The most important of these is ``BSON_NOT_UTF8`` because the BSON
object cannot be used with MongoDB if it is not valid UTF8.

To keep your code clean, you may want to check for BSON_OK only when
calling ``bson_finish()``. If the object is not valid, it will not be
finished, so it's quite important to check the return code here.

Reading BSON objects
--------------------


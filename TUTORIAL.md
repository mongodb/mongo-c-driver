# Getting started with C Driver


## Introduction

This page provides you with a little help getting started with the new MongoDB C driver.

For more information on the C API, please refer to the [online API Documentation for the C driver](http://api.mongodb.org/c/current/).


## Installation

See the relevant section below for your given operating system. This tutorial will install the newest version of the driver available from the github git repository.

### Linux, FreeBSD, Solaris, and OS X

First, ensure you have the required dependencies installed. We will assume a Fedora based operating system, but it should not be much different on your platform. OS X users often use `brew`, FreeBSD has `pkg`, and Solaris has `pkgin`.

```sh
sudo yum install git gcc automake autoconf libtool pkg-config
```

If you would like SSL support, make sure you have openssl installed.

```sh
sudo yum install openssl-devel
```

If you would like Kerberos support, make sure you have cyrus-sasl installed.

```sh
sudo yum install cyrus-sasl-devel
```

Now let's fetch the code, build, and install it. You may want to replace `--prefix` and `--libdir` with where you want the driver installed. The default is `/usr/local` and `/usr/local/lib`. However, many systems do not include this in `LD_LIBRARY_PATH` (or `DYLD_LIBRARY_PATH` in the case of OS X).

After running `./autogen.sh`, you will see the newly generated `./configure` script. It is automatically run for you by `./autogen.sh`. If you run `./configure --help`, you can see the full set of options to customize your build for your exact requirements.

```sh
git clone git://github.com/mongodb/mongo-c-driver.git
cd mongo-c-driver
./autogen.sh --prefix=/usr --libdir=/usr/lib64
gmake
sudo gmake install
```

### Windows

Install the dependencies for Windows. You will need `git` to fetch the code, and `cmake` to bootstrap the build. You will also need Visual Studio 2010 or newer. The free express version should work fine. The following assumes Visual Studio 2010.

```bat
git clone git://github.com/mongodb/mongo-c-driver.git
cd src\libbson
cmake -DCMAKE_INSTALL_PREFIX=C:\usr -G "Visual Studio 10 Win64" .
msbuild.exe ALL_BUILD.vcxproj
msbuild.exe INSTALL.vcxproj
cd ..\..
cmake -DCMAKE_INSTALL_PREFIX=C:\usr -DBSON_ROOT_DIR=C:\usr -G "Visual Studio 10 Win64" .
msbuild.exe ALL_BUILD.vcxproj
msbuild.exe INSTALL.vcxproj
```

## Making a Connection

The mongo-c-driver provides a convenient way to access MongoDB regardless of your cluster configuration. It will handle connecting to single servers, replica sets, and sharded clusters.

In the following, we will create a new `mongoc_client_t` that will be used to connect to `localhost:27017`.

```c
#include <mongoc.h>

int
main (int argc,
      char *argv[])
{
   mongoc_client_t *client;

   mongoc_init ();

   /* create a new client instance */
   client = mongoc_client_new ("mongodb://localhost:27017");

   /* now release it */
   mongoc_client_destroy (client);

   return 0;
}
```

And to compile it:

```sh
gcc -o test1 test.c $(pkg-config --cflags --libs libmongoc-1.0)
```

Alternatively, if you don't have `pkg-config` on your system, you can manually manage your include paths and libraries.

```sh
gcc -o test1 test.c -I/usr/local/include -lmongoc-1.0 -lbson-1.0
```

It is important to note that creating a new instance of `mongoc_client_t` does not immediately connect to your MongoDB instance. This is performed lazily as needed.


### Connecting to a Replica Set

To connect to a replica set, specify the replica set name using the `?replicaSet=mysetname` URI option. You can also specify a seed list if you would like to be able to connect to multiple nodes in the case that one is down when initializing the connection.

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb://host1:27017,host2:27017/?replicaSet=mysetname");
```

### Connecting to a Sharded Cluster

This is just like connecting to a replica set, but you do not need to specify a replica set name.

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb://host1:27017,host2:27017/");
```

### Connecting to a Unix Domain Socket

You can specify a path to a Unix domain socket instead of an IPv4 or IPv6 address. Simply pass the path to the socket. The socket path **MUST** end in ".sock".

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb:///tmp/mysocket.sock");
```

### Connecting to an IPv6 Address

The mongo-c-driver will automatically resolved IPv6 address from host names. But if you would like to specify the IPv6 address directly, wrap the address in `[]`.

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb://[::1]:27017/");
```

### Authentication

If you would like to use authentication with your MongoDB instance, you can simply provide the credentials as part of the URI string.

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb://username:password@localhost/");
```

### SSL

To enable SSL from your client, simply add the `?ssl=true` option to the URI.

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost/?ssl=true");
```

### Kerberos

If you are one of the people on planet Earth lucky enough to use Kerberos, you can enable that simply with the following URI. Note the escaping of the principal as it would duplicate the `@` sign in the URI.

```c
mongoc_uri_t *uri = mongoc_uri_new ("mongodb://user%40realm.cc@localhost:27017/?authmechanism=GSSAPI");
```

### Additional Connection Options

See the full variety of connection URI options at http://docs.mongodb.org/manual/reference/connection-string/.

## Getting a Database or Collection

Now that we have a client, we can fetch a handle to the collection or database using the following. Keep in mind that these functions return newly allocated structures, and therefore they need to be released when we are done using them.

```c
mongoc_collection_t *collection;
mongoc_database_t *database;

database = mongoc_client_get_database (client, "test");
collection = mongoc_client_get_collection (client, "test", "test");

mongoc_collection_destroy (collection);
mongoc_database_destroy (database);
```


## Creating BSON documents

There are two ways to create bson documents.
The first, is the imperative way.
Most C bson implementations have been similar to this.
In this model, we initialize a `bson_t`, and then add fields to it one at a time.

```c
#include <bson.h>
#include <bcon.h>

bson_t *b;

b = bson_new ();

/*
 * append ("hello": "world") to the document.
 * -1 means the string is \0 terminated.
 */
bson_append_utf8 (b, "hello", -1, "world", -1);

/*
 * This is equivalent and saves characters.
 */
BSON_APPEND_UTF8 (b, "hello", "world");

/*
 * Let's build a sub-document.
 */
bson_t child;

BSON_APPEND_DOCUMENT_BEGIN (b, "subdoc", &child);
BSON_APPEND_UTF8 (&child, "subkey", "value");
bson_append_document_end (b, &child);

/*
 * Now let's print it as a JSON string.
 */
char *str = bson_as_json (b, NULL);
printf ("%s\n", str);
bson_free (str);
```

See `bson.h` for all of the types you can append to a `bson_t`.


### Using BCON to build documents.

The imperative model of creating documents is time consuming and a lot of code.
For this reason, BCON was created.
It stands for _BSON C Object Notation_.
This uses variadic macros to simplify BSON document creation.
It has some overhead, but rarely do we find BSON document creation the bottleneck.

```c
bson_t *b;

b = BCON_NEW ("hello", BCON_UTF8 ("world"),
              "count", "{",
                 "$gt", BCON_INT32 (10),
              "}",
              "array", "[", BCON_INT32 (1), BCON_INT32 (2), "]",
              "another", BCON_UTF8 ("string"));
```

Notice that you can create arrays, subdocuments, and arbitrary fields.


## Inserting and querying a document

Now that we know how to get a handle to a database and collection, let's insert a document and then query it back!

```c
#include <bcon.h>
#include <bson.h>
#include <mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc,
      char *argv[])
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *item;
   bson_error_t error;
   bson_oid_t oid;
   bson_t *query;
   bson_t *doc;
   char *str;
   bool r;

   mongoc_init ();

   /* get a handle to our collection */
   client = mongoc_client_new ("mongodb://localhost:27017");
   collection = mongoc_client_get_collection (client, "test", "test");

   /* insert a document */
   bson_oid_init (&oid, NULL);
   doc = BCON_NEW ("_id", BCON_OID (&oid),
                   "hello", BCON_UTF8 ("world!"));
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, NULL, &error);
   if (!r) {
      fprintf (stderr, "%s\n", error.message);
      return EXIT_FAILURE;
   }

   /* build a query to execute */
   query = BCON_NEW ("_id", BCON_OID (&oid));

   /* execute the query and iterate the results */
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);
   while (mongoc_cursor_next (cursor, &item)) {
      str = bson_as_json (item, NULL);
      printf ("%s\n", str);
      bson_free (str);
   }

   /* release everything */
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (query);
   bson_destroy (doc);

   return 0;
}
```

## BCON simplified document format

Creating BSON documents in the imperative way is time consuming and can be prone to error. **BSON C Notation**, or `BCON` for short, is a way to create documents that looks much closer to the document format you are trying to create. While the type safety is less than your typical imperative code, it can make for much cleaner C code.

```c
#include <bcon.h>
#include <bson.h>
#include <stdio.h>

int
main (int argc,
      char *argv[])
{
   bson_t *doc;

   doc = BCON_NEW ("name", BCON_UTF8 ("Babe Ruth"),
                   "batting_average", BCON_DOUBLE (.342),
                   "hits", BCON_INT32 (2873),
                   "home_runs", BCON_INT32 (714),
                   "rbi", BCON_INT32 (2213),
                   "nicknames", "[",
                      BCON_UTF8 ("the Sultan of Swat"),
                      BCON_UTF8 ("the Bambino"),
                   "]");

   str = bson_as_json (doc, NULL);
   printf ("%s\n", str);
   bson_free (str);

   bson_destroy (doc);

   return 0;
}
```


## Counting documents in a collection

To count the number of documents in a collection we need to build a query selector. An empty query selector would count all documents in a collection.

```c
#include <mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc,
      char *argv[])
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   int64_t count;
   bson_t query;
   bool r;

   mongoc_init ();

   /* get a handle to our collection */
   client = mongoc_client_new ("mongodb://localhost:27017");
   collection = mongoc_client_get_collection (client, "test", "test");

   /* count all documents matching {hello: "world!"} */
   bson_init (&query);
   BSON_APPEND_UTF8 (&query, "hello", "world!");
   count = mongoc_collection_count (collection, MONGOC_QUERY_NONE, &query, 0, 0, NULL, &error);
   if (count < 0) {
      fprintf (stderr, "%s\n", error.message);
      return EXIT_FAILURE;
   }

   printf ("Found %"PRId64" documents in %s\n",
           count,
           mongoc_collection_get_name (collection));

   /* release everything */
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&query);

   return 0;
}
```


## Threading

The mongo-c-driver is threading unaware for the vast majority of things. This means it is up to you to guarantee thread-safety. However, `mongoc_client_pool_t` is thread-safe. It is used to fetch a `mongoc_client_t` in a thread-safe manner. After retrieving a client from the pool, the client structure should be considered owned by the calling thread. When the thread is finished, it should be placed back into the pool.

```c
#include <mongoc.h>
#include <pthread.h>

#define N_THREADS 10

static void *
worker (void *data)
{
   mongoc_client_pool_t *pool = data;
   mongoc_client_t *client;

   client = mongoc_client_pool_pop (pool);

   /* do something */

   mongoc_client_pool_push (pool, client);

   return NULL;
}

int
main (int argc,
      char *argv[])
{
   mongoc_client_pool_t *pool;
   mongoc_uri_t *uri;
   pthread_t threads[N_THREADS];

   mongoc_init ();

   uri = mongoc_uri_new ("mongodb://localhost/");
   pool = mongoc_client_pool_new (uri);

   for (i = 0; i < N_THREADS; i++) {
      pthread_create (&threads[i], NULL, worker, pool);
   }

   for (i = 0; i < N_THREADS; i++) {
      pthread_join (threads[i], NULL);
   }

   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);

   return 0;
}
```


## Executing a Command

There are helpers to make it easy to execute commands. The helper exists for client, database, and collection structures. Alternatively, you can use the `_simple()` variant if you do not need a cursor back from the result. This is convenient for commands that return a single document.

```c
#include <bcon.h>
#include <bson.h>
#include <mongoc.h>
#include <stdio.h>

int
main (int argc,
      char *argv[])
{
   mongoc_client_t *client;
   bson_t *command;
   bson_t reply;
   char *str;
   bool r;

   mongoc_init ();

   client = mongoc_client_new ("mongodb://localhost:27017/");

   command = BCON_NEW ("ping", BCON_INT32 (1));

   r = mongoc_client_command_simple (client, "admin", command, NULL, &reply, &error);
   if (!r) {
      printf ("%s\n", error.message);
      return 1;
   }

   str = bson_as_json (&reply, NULL);
   printf ("%s\n", str);
   bson_free (str);

   bson_destroy (&reply);
   bson_destroy (command);
   mongoc_client_destroy (client);

   return 0;
}
```


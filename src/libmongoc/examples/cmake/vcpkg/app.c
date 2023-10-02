#include <mongoc/mongoc.h>

#include <stdio.h>

int
main ()
{
   fprintf (stdout, "Linked with libmongoc %s\n", mongoc_get_version ());
}

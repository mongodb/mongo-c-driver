#include <mongoc/mongoc.h>

#include <stdio.h>

int
main (void)
{
   fprintf (stdout, "Linked with libmongoc %s\n", mongoc_get_version ());
   return 0;
}

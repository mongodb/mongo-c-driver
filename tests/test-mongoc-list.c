#include <mongoc.h>
#include <mongoc-list-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_list_basic (void)
{
   mongoc_list_t *l;

   l = _mongoc_list_append(NULL, (void *)1ULL);
   l = _mongoc_list_append(l, (void *)2ULL);
   l = _mongoc_list_append(l, (void *)3ULL);
   l = _mongoc_list_prepend(l, (void *)4ULL);

   assert(l);
   assert(l->next);
   assert(l->next->next);
   assert(l->next->next->next);
   assert(!l->next->next->next->next);

   assert(l->data == (void *)4ULL);
   assert(l->next->data == (void *)1ULL);
   assert(l->next->next->data == (void *)2ULL);
   assert(l->next->next->next->data == (void *)3ULL);

   l = _mongoc_list_remove(l, (void *)4ULL);
   assert(l->data == (void *)1ULL);
   assert(l->next->data == (void *)2ULL);
   assert(l->next->next->data == (void *)3ULL);

   l = _mongoc_list_remove(l, (void *)2ULL);
   assert(l->data == (void *)1ULL);
   assert(l->next->data == (void *)3ULL);
   assert(!l->next->next);

   l = _mongoc_list_remove(l, (void *)1ULL);
   assert(l->data == (void *)3ULL);
   assert(!l->next);

   l = _mongoc_list_remove(l, (void *)3ULL);
   assert(!l);

   _mongoc_list_destroy(l);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/list/basic", test_mongoc_list_basic);

   return 0;
}



#include "./TestSuite.h"

#include <mongoc/mongoc-shared-private.h>

typedef struct {
   int value;
   int *store_value_on_dtor;
} my_value;

my_value *
my_value_new ()
{
   my_value *p = bson_malloc0 (sizeof (my_value));
   p->value = 42;
   p->store_value_on_dtor = NULL;
   return p;
}

void
my_value_free (my_value *ptr)
{
   if (ptr->store_value_on_dtor) {
      *ptr->store_value_on_dtor = ptr->value;
   }
   ptr->value = 0;
   ptr->store_value_on_dtor = NULL;
   bson_free (ptr);
}

void
my_value_free_v (void *ptr)
{
   my_value_free ((my_value *) (ptr));
}

static void
test_simple ()
{
   int destroyed_value = 0;
   mongoc_shared_ptr ptr = {0};

   ASSERT (mongoc_shared_ptr_is_null (ptr));
   ptr = mongoc_shared_ptr_create (my_value_new (), my_value_free_v);
   ASSERT (!mongoc_shared_ptr_is_null (ptr));

   ASSERT_CMPINT (mongoc_shared_ptr_refcount (ptr), ==, 1);

   mongoc_shared_ptr ptr2 = mongoc_shared_ptr_take (ptr);

   ASSERT (ptr.ptr == ptr2.ptr);
   ASSERT (ptr._aux == ptr._aux);

   MONGOC_Shared_Take (my_value, valptr, ptr);
   valptr->store_value_on_dtor = &destroyed_value;
   valptr->value = 133;
   MONGOC_Shared_Release (valptr);
   /* Value hasn't changed yet */
   ASSERT_CMPINT (destroyed_value, ==, 0);

   /* Now drop the final reference */
   mongoc_shared_ptr_release (&ptr);
   /* Check that the pointer is empty */
   ASSERT (mongoc_shared_ptr_is_null (ptr));

   /* Still not yet */
   ASSERT_CMPINT (destroyed_value, ==, 0);

   /* CHeck that the existing pointer is okay */
   ASSERT_CMPINT (((my_value *) ptr2.ptr)->value, ==, 133);

   /* Drop the last one */
   mongoc_shared_ptr_release (&ptr2);
   ASSERT (mongoc_shared_ptr_is_null (ptr2));

   /* Now it was destroyed and set */
   ASSERT_CMPINT (destroyed_value, ==, 133);
}

void
test_shared_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/shared/simple", test_simple);
}

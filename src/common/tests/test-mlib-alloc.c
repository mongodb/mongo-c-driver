#include <mlib/alloc.h>
#include <mlib/test.h>

const mlib_allocator *
mlib_test_allocator_from_peer(void);

void
mlib_test_set_allocator_from_peer(const mlib_allocator *allocator);

void
mlib_test_restore_allocator_from_peer(void);

void *
mlib_test_allocate_from_peer(size_t size);

#ifdef MLIB_TEST_CXX
const mlib_allocator *
mlib_test_allocator_from_cxx(void);

void
mlib_test_set_allocator_from_cxx(const mlib_allocator *allocator);

void
mlib_test_restore_allocator_from_cxx(void);

void *
mlib_test_allocate_from_cxx(size_t size);
#endif

/* An allocation attempt observes whether a different TU uses our installed state. */
static void *
count_allocation(void *userdata, void *ptr, size_t size, size_t previous_size, size_t *out_new_size)
{
   (void)ptr;
   (void)size;
   (void)previous_size;
   (void)out_new_size;
   ++*(size_t *)userdata;
   return NULL;
}

int
main(void)
{
   const mlib_allocator *const view = mlib_get_allocator();
   const mlib_allocator original = *view;
   size_t calls = 0;
   const mlib_allocator allocator = {&calls, count_allocation};

   mlib_check(mlib_test_allocator_from_peer(), ptr_eq, view);
   mlib_test_set_allocator_from_peer(&allocator);
   mlib_check(view->userdata, ptr_eq, &calls);
   mlib_check(mlib_allocate(1), ptr_eq, NULL);
   mlib_check(calls, eq, 1);

   mlib_restore_allocator();
   mlib_check(mlib_test_allocator_from_peer()->userdata, ptr_eq, original.userdata);
   mlib_check(mlib_test_allocator_from_peer()->reallocate == original.reallocate);

   mlib_set_allocator(&allocator);
   mlib_check(mlib_test_allocate_from_peer(1), ptr_eq, NULL);
   mlib_check(calls, eq, 2);
   mlib_test_restore_allocator_from_peer();
   mlib_check(mlib_get_allocator(), ptr_eq, view);
   mlib_check(view->userdata, ptr_eq, original.userdata);
   mlib_check(view->reallocate == original.reallocate);

#ifdef MLIB_TEST_CXX
   mlib_check(mlib_test_allocator_from_cxx(), ptr_eq, view);
   mlib_test_set_allocator_from_cxx(&allocator);
   mlib_check(view->userdata, ptr_eq, &calls);
   mlib_check(mlib_test_allocate_from_peer(1), ptr_eq, NULL);
   mlib_check(calls, eq, 3);

   mlib_restore_allocator();
   mlib_check(mlib_test_allocator_from_cxx()->userdata, ptr_eq, original.userdata);
   mlib_check(mlib_test_allocator_from_cxx()->reallocate == original.reallocate);

   mlib_set_allocator(&allocator);
   mlib_check(mlib_test_allocate_from_cxx(1), ptr_eq, NULL);
   mlib_check(calls, eq, 4);
   mlib_test_restore_allocator_from_cxx();
   mlib_check(mlib_get_allocator(), ptr_eq, view);
   mlib_check(view->userdata, ptr_eq, original.userdata);
   mlib_check(view->reallocate == original.reallocate);
#endif
   return 0;
}

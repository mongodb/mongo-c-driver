#include <mlib/alloc.h>
#include <mlib/test.h>

struct _mlib_allocator_state *
mlib_test_allocator_from_peer(void);

void
mlib_test_set_allocator_from_peer(mlib_reallocate_fn reallocate, void *userdata);

void
mlib_test_restore_allocator_from_peer(void);

void *
mlib_test_allocate_from_peer(size_t size);

#ifdef MLIB_TEST_CXX
struct _mlib_allocator_state *
mlib_test_allocator_from_cxx(void);

void
mlib_test_set_allocator_from_cxx(mlib_reallocate_fn reallocate, void *userdata);

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
   struct _mlib_allocator_state *const state = _mlib_get_global_singleton_allocator_object_storage();
   const struct _mlib_allocator *const view = &state->current;
   const struct _mlib_allocator original = *view;
   size_t calls = 0;

   mlib_check(mlib_test_allocator_from_peer(), ptr_eq, state);
   mlib_test_set_allocator_from_peer(count_allocation, &calls);
   mlib_check(view->userdata, ptr_eq, &calls);
   mlib_check(mlib_allocate(1), ptr_eq, NULL);
   mlib_check(calls, eq, 1);

   mlib_restore_allocator();
   mlib_check(mlib_test_allocator_from_peer()->current.userdata, ptr_eq, original.userdata);
   mlib_check(mlib_test_allocator_from_peer()->current.reallocate == original.reallocate);

   mlib_set_allocator(count_allocation, &calls);
   mlib_check(mlib_test_allocate_from_peer(1), ptr_eq, NULL);
   mlib_check(calls, eq, 2);
   mlib_test_restore_allocator_from_peer();
   mlib_check(_mlib_get_global_singleton_allocator_object_storage(), ptr_eq, state);
   mlib_check(view->userdata, ptr_eq, original.userdata);
   mlib_check(view->reallocate == original.reallocate);

#ifdef MLIB_TEST_CXX
   mlib_check(mlib_test_allocator_from_cxx(), ptr_eq, state);
   mlib_test_set_allocator_from_cxx(count_allocation, &calls);
   mlib_check(view->userdata, ptr_eq, &calls);
   mlib_check(mlib_test_allocate_from_peer(1), ptr_eq, NULL);
   mlib_check(calls, eq, 3);

   mlib_restore_allocator();
   mlib_check(mlib_test_allocator_from_cxx()->current.userdata, ptr_eq, original.userdata);
   mlib_check(mlib_test_allocator_from_cxx()->current.reallocate == original.reallocate);

   mlib_set_allocator(count_allocation, &calls);
   mlib_check(mlib_test_allocate_from_cxx(1), ptr_eq, NULL);
   mlib_check(calls, eq, 4);
   mlib_test_restore_allocator_from_cxx();
   mlib_check(_mlib_get_global_singleton_allocator_object_storage(), ptr_eq, state);
   mlib_check(view->userdata, ptr_eq, original.userdata);
   mlib_check(view->reallocate == original.reallocate);
#endif
   return 0;
}

#include <mlib/alloc.h>
#include <mlib/test.h>

struct _mlib_allocator_state *
alloc_test_peer_get_state(void);

void
alloc_test_peer_set_allocator(mlib_reallocate_fn reallocate, void *userdata);

void
alloc_test_peer_restore_allocator(void);

void *
alloc_test_peer_allocate(size_t size);

#ifdef MLIB_TEST_CXX
struct _mlib_allocator_state *
alloc_test_cxx_get_state(void);

void
alloc_test_cxx_set_allocator(mlib_reallocate_fn reallocate, void *userdata);

void
alloc_test_cxx_restore_allocator(void);

void *
alloc_test_cxx_allocate(size_t size);
#endif

// Increment the counter supplied through `userdata`, then return `NULL` without
// allocating. The counter records requests handled by the installed callback/context
// pair; these requests leave no allocations to release before switching.
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
   // Keep the original state address and a live view of the shared allocator
   // configuration. Copy the initial callback/context pair separately so restoration
   // can be checked without the expected values changing with the live view.
   struct _mlib_allocator_state *const state = _mlib_get_global_singleton_allocator_object_storage();
   const struct _mlib_allocator *const view = &state->current;
   const struct _mlib_allocator original = *view;
   size_t calls = 0;

   // The other C translation unit must resolve to the same storage. Installing
   // there must update this translation unit's live view and route its next
   // allocation through the shared callback/context exactly once.
   mlib_check(alloc_test_peer_get_state(), ptr_eq, state);
   alloc_test_peer_set_allocator(count_allocation, &calls);
   mlib_check(view->userdata, ptr_eq, &calls);
   mlib_check(mlib_allocate(1), ptr_eq, NULL);
   mlib_check(calls, eq, 1);

   // Restoring here must also restore the callback and context observed by the C peer.
   mlib_restore_allocator();
   mlib_check(alloc_test_peer_get_state()->current.userdata, ptr_eq, original.userdata);
   mlib_check(alloc_test_peer_get_state()->current.reallocate == original.reallocate);

   // Check the reverse direction: install here and allocate in the C peer.
   // The cumulative count must advance from one to two.
   mlib_set_allocator(count_allocation, &calls);
   mlib_check(alloc_test_peer_allocate(1), ptr_eq, NULL);
   mlib_check(calls, eq, 2);
   // Restoring in the peer must preserve the singleton address and restore both
   // fields seen through the original live view.
   alloc_test_peer_restore_allocator();
   mlib_check(_mlib_get_global_singleton_allocator_object_storage(), ptr_eq, state);
   mlib_check(view->userdata, ptr_eq, original.userdata);
   mlib_check(view->reallocate == original.reallocate);

#ifdef MLIB_TEST_CXX
   // The C++ translation unit must share the C state as well. Install from C++,
   // observe the context here, and dispatch from the C peer; the third counted
   // request checks that all three translation units use the same configuration.
   mlib_check(alloc_test_cxx_get_state(), ptr_eq, state);
   alloc_test_cxx_set_allocator(count_allocation, &calls);
   mlib_check(view->userdata, ptr_eq, &calls);
   mlib_check(alloc_test_peer_allocate(1), ptr_eq, NULL);
   mlib_check(calls, eq, 3);

   // Restoring from C must be visible to the C++ peer as the original callback/context pair.
   mlib_restore_allocator();
   mlib_check(alloc_test_cxx_get_state()->current.userdata, ptr_eq, original.userdata);
   mlib_check(alloc_test_cxx_get_state()->current.reallocate == original.reallocate);

   // Finally install from C and allocate from C++; the fourth counted request
   // checks dispatch across the language boundary in the other direction.
   mlib_set_allocator(count_allocation, &calls);
   mlib_check(alloc_test_cxx_allocate(1), ptr_eq, NULL);
   mlib_check(calls, eq, 4);
   // Restoring from C++ must leave the same singleton address and original
   // callback/context pair visible here.
   alloc_test_cxx_restore_allocator();
   mlib_check(_mlib_get_global_singleton_allocator_object_storage(), ptr_eq, state);
   mlib_check(view->userdata, ptr_eq, original.userdata);
   mlib_check(view->reallocate == original.reallocate);
#endif
   return 0;
}

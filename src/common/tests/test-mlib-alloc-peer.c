#include <mlib/alloc.h>

const mlib_allocator *
mlib_test_allocator_from_peer(void)
{
   return mlib_get_allocator();
}

void
mlib_test_set_allocator_from_peer(const mlib_allocator *allocator)
{
   mlib_set_allocator(allocator);
}

void
mlib_test_restore_allocator_from_peer(void)
{
   mlib_restore_allocator();
}

void *
mlib_test_allocate_from_peer(size_t size)
{
   return mlib_allocate(size);
}

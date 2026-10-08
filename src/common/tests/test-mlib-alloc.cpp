#include <mlib/alloc.h>
extern "C" const mlib_allocator *
mlib_test_allocator_from_cxx(void)
{
   return mlib_get_allocator();
}

extern "C" void
mlib_test_set_allocator_from_cxx(const mlib_allocator *allocator)
{
   mlib_set_allocator(allocator);
}

extern "C" void
mlib_test_restore_allocator_from_cxx(void)
{
   mlib_restore_allocator();
}

extern "C" void *
mlib_test_allocate_from_cxx(size_t size)
{
   return mlib_allocate(size);
}

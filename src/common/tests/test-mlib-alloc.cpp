#include <mlib/alloc.h>
extern "C" struct _mlib_allocator_state *
alloc_test_cxx_get_state(void)
{
   return _mlib_get_global_singleton_allocator_object_storage();
}

extern "C" void
alloc_test_cxx_set_allocator(mlib_reallocate_fn reallocate, void *userdata)
{
   mlib_set_allocator(reallocate, userdata);
}

extern "C" void
alloc_test_cxx_restore_allocator(void)
{
   mlib_restore_allocator();
}

extern "C" void *
alloc_test_cxx_allocate(size_t size)
{
   return mlib_allocate(size);
}

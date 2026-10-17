#include <mlib/alloc.h>
extern "C" struct _mlib_allocator_state *
mlib_test_allocator_from_cxx(void)
{
   return _mlib_get_global_singleton_allocator_object_storage();
}

extern "C" void
mlib_test_set_allocator_from_cxx(mlib_reallocate_fn reallocate, void *userdata)
{
   mlib_set_allocator(reallocate, userdata);
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

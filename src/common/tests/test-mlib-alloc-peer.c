#include <mlib/alloc.h>

struct _mlib_allocator_state *
mlib_test_allocator_from_peer(void)
{
   return _mlib_get_global_singleton_allocator_object_storage();
}

void
mlib_test_set_allocator_from_peer(mlib_reallocate_fn reallocate, void *userdata)
{
   mlib_set_allocator(reallocate, userdata);
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

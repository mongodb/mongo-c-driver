#include <mlib/alloc.h>

struct _mlib_allocator_state *
alloc_test_peer_get_state(void)
{
   return _mlib_get_global_singleton_allocator_object_storage();
}

void
alloc_test_peer_set_allocator(mlib_reallocate_fn reallocate, void *userdata)
{
   mlib_set_allocator(reallocate, userdata);
}

void
alloc_test_peer_restore_allocator(void)
{
   mlib_restore_allocator();
}

void *
alloc_test_peer_allocate(size_t size)
{
   return mlib_allocate(size);
}

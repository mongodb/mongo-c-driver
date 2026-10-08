/**
 * @file mlib/alloc.h
 * @brief Global, replaceable allocation for mlib.
 *
 * @copyright Copyright 2009-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MLIB_ALLOC_H_INCLUDED
#define MLIB_ALLOC_H_INCLUDED

#include <mlib/config.h>
#include <mlib/test.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

mlib_extern_c_begin();

/**
 * @brief Configuration for the global mlib allocator, not an object-associated allocator.
 *
 * The callback must support ordinary malloc alignment. Over-aligned allocations
 * are not supported. Allocation failures must return NULL rather than terminate.
 * Concurrent allocation requires a thread-safe callback and context.
 */
typedef struct mlib_allocator {
   /** Borrowed context, passed unchanged to the callback. May be NULL. */
   void *userdata;

   /**
    * @brief Allocate, resize, or release a region. Must be non-NULL.
    *
    * If prev_ptr is NULL, previous_size must be zero. Otherwise, prev_ptr must
    * have been allocated by this allocator, and previous_size must be its
    * previous requested size (not the optional reported usable size).
    *
    * A positive requested_size requests a region of at least that size.
    * Successful resizing preserves the first min(previous_size, requested_size)
    * bytes. The old pointer is invalidated, even if its address is unchanged.
    * If out_new_size is non-NULL, success writes the usable region size.
    *
    * A failed positive-size request returns NULL, leaves the old region intact,
    * and does not modify out_new_size.
    *
    * A zero requested_size releases prev_ptr, returns NULL, and writes zero to
    * out_new_size if supplied. A NULL prev_ptr with zero requested_size is a
    * no-op. The callback must not throw when compiled as C++.
    */
   void *(*reallocate)(
      void *userdata, void *prev_ptr, size_t requested_size, size_t previous_size, size_t *out_new_size)mlib_noexcept;
} mlib_allocator;

/**
 * @internal
 * @brief The singleton allocate object state. The one living instance of this
 * object is obtained through the conspiciously named internal function below.
 */
struct _mlib_allocator_state {
   // The actual current allocator used by the program
   mlib_allocator current;
   /// The default `stdlib.h` allocator
   const mlib_allocator default_allocator;
};

/**
 * @internal
 * @brief The single exported access point for allocator state.
 *
 * Defined only in the owning TU. Its function-local static provides the home for
 * both configurations; inline wrappers in other images reference that same home.
 *
 * The accessor returns an invariant address without side effects or unwinding.
 * Its const annotation describes the accessor, not the mutable pointed-to state.
 *
 * The `const` and `nothrow` attributes are extremely useful hints to the optimizer
 * that allow it to assume the function has no effects and always returns normally.
 */
MLIB_IF_GNU_LIKE(__attribute__((const, nothrow, visibility("default"))))
struct _mlib_allocator_state *
_mlib_get_global_singleton_allocator_object_storage(void) mlib_noexcept;

/**
 * @brief Obtain a borrowed, read-only view of the current global allocator.
 *
 * The pointer remains valid, but installing or restoring the allocator changes
 * its contents. It is not a snapshot. Do not call the callback directly; use the
 * allocation helpers below.
 */
static inline const mlib_allocator *
mlib_get_allocator(void) mlib_noexcept
{
   return &_mlib_get_global_singleton_allocator_object_storage()->current;
}

/**
 * @brief Copy a configuration into the global allocator.
 *
 * allocator and its callback must be non-NULL; invalid configurations terminate
 * with a diagnostic. The source struct need not remain alive after this call,
 * but its callback and borrowed context must remain valid while installed.
 *
 * Before installing or restoring, release all outstanding allocations obtained
 * through the current allocator. Configuration changes must not overlap any
 * other allocator operation, including reading the configuration.
 */
static inline void
mlib_set_allocator(const mlib_allocator *allocator) mlib_noexcept
{
   mlib_check(allocator != NULL, because, "Setting a null allocator is not valid");
   mlib_check(allocator->reallocate != NULL, because, "The reallocation function is required");
   _mlib_get_global_singleton_allocator_object_storage()->current = *allocator;
}

/** Restore the stdlib allocator, subject to the same restrictions as installation. */
static inline void
mlib_restore_allocator(void) mlib_noexcept
{
   struct _mlib_allocator_state *const state = _mlib_get_global_singleton_allocator_object_storage();
   state->current = state->default_allocator;
}

/**
 * @brief Resize through the current allocator, returning NULL on allocation failure.
 *
 * Parameters and results follow mlib_allocator.reallocate. Zero size explicitly
 * requests deallocation; this does not depend on the platform's realloc(p, 0)
 * behavior. A NULL pointer and zero size do not invoke the callback.
 */
static inline void *
mlib_reallocate(void *prev_ptr, size_t new_size, size_t previous_size, size_t *out_new_size) mlib_noexcept
{
   if (!prev_ptr && new_size == 0) {
      if (out_new_size) {
         *out_new_size = 0;
      }
      return NULL;
   }
   const mlib_allocator *const allocator = mlib_get_allocator();
   return allocator->reallocate(allocator->userdata, prev_ptr, new_size, previous_size, out_new_size);
}

/** Allocate uninitialized memory through the current allocator. Zero size returns NULL. */
static inline void *
mlib_allocate(size_t size) mlib_noexcept
{
   return mlib_reallocate(NULL, size, 0, NULL);
}

/**
 * @brief Release memory through the current allocator.
 *
 * previous_size must be the original requested size. A NULL pointer is a no-op.
 */
static inline void
mlib_deallocate(void *ptr, size_t previous_size) mlib_noexcept
{
   if (ptr) {
      mlib_reallocate(ptr, 0, previous_size, NULL);
   }
}

mlib_extern_c_end();

#endif // MLIB_ALLOC_H_INCLUDED

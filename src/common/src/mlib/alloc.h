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

#include <mlib/platform/compiler.h>
#include <mlib/platform/language.h>
#include <mlib/static_assert.h>
#include <mlib/test.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

mlib_extern_c_begin();

/**
 * @brief Callback implementing allocation, resizing, and release for the global mlib allocator.
 *
 * Implement the allocation contract documented by `mlib_reallocate`, with
 * `requested_size` corresponding to its `new_size` parameter. `userdata` is the
 * borrowed context supplied to `mlib_set_allocator`.
 *
 * Concurrent allocation requires a thread-safe callback and context. The
 * callback must not throw when compiled as C++.
 */
typedef void *(*mlib_reallocate_fn)(
   void *userdata, void *prev_ptr, size_t requested_size, size_t previous_size, size_t *out_new_size);

/** @internal Callback and borrowed context for the global allocator. */
struct _mlib_allocator {
   void *userdata;
   mlib_reallocate_fn reallocate;
};

/**
 * @internal
 * @brief Mutable global configuration and the immutable stdlib configuration used to restore it.
 *
 * Keeping both in the allocator implementation translation unit lets inline
 * helpers share state instead of creating a separate allocator in each
 * translation unit.
 */
struct _mlib_allocator_state {
   /// Configuration used for subsequent allocation and deallocation requests.
   struct _mlib_allocator current;
   /// Initial stdlib configuration, retained independently of later installations.
   const struct _mlib_allocator default_allocator;
};

/**
 * @internal
 * @brief The exported access point for allocator state.
 *
 * Defined only in the allocator implementation translation unit, whose
 * function-local static stores both configurations. Callers linked to that owner
 * obtain the same state; defining another owner in a separate runtime image can
 * create separate state.
 *
 * The accessor returns a stable address without side effects or unwinding.
 * Its `const` annotation describes the accessor, not the mutable pointed-to state:
 * callers may reuse the address, but allocator installation can change the
 * configuration at that address.
 */
MLIB_IF_GNU_LIKE(__attribute__((const, nothrow, visibility("default"))))
struct _mlib_allocator_state *
_mlib_get_global_singleton_allocator_object_storage(void) mlib_noexcept;

/**
 * @brief Install a callback and borrowed context as the global allocator.
 *
 * `reallocate` must be non-`NULL`; a null callback terminates with a diagnostic.
 * `userdata` may be `NULL` and is passed unchanged to the callback. The callback and
 * its borrowed context must remain valid while installed.
 *
 * Allocations do not retain their originating allocator; later resize and
 * deallocation requests use the current configuration. Before installing or
 * restoring, release all outstanding allocations obtained through the current
 * allocator. Configuration changes must not overlap any other allocator operation.
 */
static inline void
mlib_set_allocator(mlib_reallocate_fn reallocate, void *userdata) mlib_noexcept
{
   mlib_check(reallocate != NULL, because, "The reallocation function is required");
   struct _mlib_allocator *const allocator = &_mlib_get_global_singleton_allocator_object_storage()->current;
   allocator->reallocate = reallocate;
   allocator->userdata = userdata;
}

/**
 * @brief Restore the stdlib allocator, not a previously installed custom allocator.
 *
 * The lifetime and synchronization restrictions on `mlib_set_allocator` also apply here.
 */
static inline void
mlib_restore_allocator(void) mlib_noexcept
{
   struct _mlib_allocator_state *const state = _mlib_get_global_singleton_allocator_object_storage();
   state->current = state->default_allocator;
}

/**
 * @brief Allocate, resize, or release memory through the current allocator.
 *
 * Allocations support ordinary `malloc` alignment; over-aligned allocations are
 * not supported.
 *
 * If `prev_ptr` is `NULL`, `previous_size` must be zero. Otherwise, `prev_ptr` must
 * have been allocated by the current allocator, and `previous_size` must be its
 * previous requested byte size (not the optional reported usable size).
 *
 * A positive `new_size` requests a region of at least that many bytes.
 * Successful resizing preserves the first `min(previous_size, new_size)` bytes.
 * The old pointer is invalidated, even if its address is unchanged. If
 * `out_new_size` is non-`NULL`, success writes the usable region size.
 *
 * A failed positive-size request returns `NULL` rather than terminating, leaves
 * the old region intact, and does not modify `out_new_size`.
 *
 * A zero `new_size` releases `prev_ptr`, returns `NULL`, and writes zero to `out_new_size`
 * if supplied. This explicitly requests deallocation and does not depend on the
 * platform's `realloc(p, 0)` behavior. A `NULL` pointer and zero size release nothing
 * and do not invoke the callback, but still report zero through `out_new_size`.
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
   // Copy the callback and context together for this dispatch. This is not
   // synchronization; configuration changes must still be excluded during
   // allocator operations.
   const struct _mlib_allocator allocator = _mlib_get_global_singleton_allocator_object_storage()->current;
   return allocator.reallocate(allocator.userdata, prev_ptr, new_size, previous_size, out_new_size);
}

/**
 * @brief Allocate `size` bytes of uninitialized memory through the current allocator.
 *
 * Returns `NULL` on allocation failure or when `size` is zero. Release a successful
 * allocation with `mlib_deallocate`, or resize it with `mlib_reallocate`.
 */
static inline void *
mlib_allocate(size_t size) mlib_noexcept
{
   return mlib_reallocate(NULL, size, 0, NULL);
}

/**
 * @brief Release memory through the current allocator.
 *
 * For a non-`NULL` `ptr`, `previous_size` must be the byte size requested by the most
 * recent successful allocation or resize, not an allocator-reported usable size.
 * A `NULL` pointer is a no-op.
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

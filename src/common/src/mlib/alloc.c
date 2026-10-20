/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <mlib/platform/compiler.h>
#include <mlib/platform/language.h>

// MSVC requires the export annotation before the header's undecorated declaration.
// MinGW relies on automatic exports, which an explicit dllexport would disable.
#if mlib_is_msvc()
__declspec(dllexport) struct _mlib_allocator_state *
_mlib_get_global_singleton_allocator_object_storage(void) mlib_noexcept;
#endif

#include <mlib/alloc.h>

#include <stdlib.h>

static void *
_mlib_stdlib_reallocate(void *userdata, void *prev_ptr, size_t size, size_t previous_size, size_t *out_new_size)
{
   // The stdlib allocator needs neither a user context nor previous_size:
   // realloc and free obtain allocation metadata themselves.
   (void)userdata;
   (void)previous_size;
   // Handle release separately: realloc(p, 0) is not a portable substitute for
   // free. This path must return NULL and report zero through out_new_size when supplied.
   if (size == 0) {
      free(prev_ptr);
      if (out_new_size) {
         *out_new_size = 0;
      }
      return NULL;
   }
   // For a positive size, realloc handles both a fresh allocation and a resize.
   // On failure it leaves the old allocation intact, so return its result without
   // freeing prev_ptr.
   void *const ptr = realloc(prev_ptr, size);
   // Report the requested size rather than trying to discover extra usable
   // capacity. Leave the caller's output untouched on failure.
   if (ptr && out_new_size) {
      *out_new_size = size;
   }
   return ptr;
}

struct _mlib_allocator_state *
_mlib_get_global_singleton_allocator_object_storage(void)
{
   // This out-of-line definition owns the storage referenced by the inline
   // helpers. Static initialization supplies a usable stdlib allocator without
   // a separate initialization call.
   static struct _mlib_allocator_state state = {
      // Start both configurations with the stdlib callback. Installations replace
      // only current, leaving default_allocator available for restoration.
      .current = {NULL, _mlib_stdlib_reallocate},
      .default_allocator = {NULL, _mlib_stdlib_reallocate},
   };
   return &state;
}

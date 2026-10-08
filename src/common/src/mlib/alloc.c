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

#include <mlib/alloc.h>

#include <stdlib.h>

static void *
_mlib_stdlib_reallocate(void *userdata, void *prev_ptr, size_t size, size_t previous_size, size_t *out_new_size)
{
   (void)userdata;
   (void)previous_size;
   if (size == 0) {
      /* realloc(p, 0) is not a portable substitute for explicit deallocation. */
      free(prev_ptr);
      if (out_new_size) {
         *out_new_size = 0;
      }
      return NULL;
   }
   void *const ptr = realloc(prev_ptr, size);
   if (ptr && out_new_size) {
      *out_new_size = size;
   }
   return ptr;
}

// Export this symbol on Windows so that it is visible as part of the DLL API
MLIB_IF_WIN32(__declspec(dllexport))
struct _mlib_allocator_state *
_mlib_get_global_singleton_allocator_object_storage(void)
{
   static struct _mlib_allocator_state state = {
      {NULL, _mlib_stdlib_reallocate},
      {NULL, _mlib_stdlib_reallocate},
   };
   return &state;
}

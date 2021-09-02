/*
 * Copyright 2021 MongoDB, Inc.
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

#include "./mongoc-shared-private.h"

#include "common-thread-private.h"
#include <bson/bson.h>

#include <assert.h>

typedef struct _mongoc_shared_ptr_aux {
   int refcount;
   void (*dtor) (void *);
   void *managed;
} _mongoc_shared_ptr_aux;

static void
_release_aux (_mongoc_shared_ptr_aux *aux)
{
   aux->dtor (aux->managed);
   bson_free (aux);
}

static bson_mutex_t g_shared_ptr_mtx;
static bson_once_t g_shared_ptr_mtx_init_once = BSON_ONCE_INIT;

static BSON_ONCE_FUN (_init_mtx)
{
   bson_mutex_init (&g_shared_ptr_mtx);
}

static void
_shared_ptr_spin_lock ()
{
   bson_mutex_lock (&g_shared_ptr_mtx);
}

static void
_shared_ptr_spin_unlock ()
{
   bson_mutex_unlock (&g_shared_ptr_mtx);
}

void
mongoc_shared_ptr_rebind_raw (mongoc_shared_ptr *const ptr,
                              void *const pointee,
                              void (*const dtor) (void *))
{
   assert (ptr && "NULL given to mongoc_shared_ptr_rebind_raw()");
   if (!mongoc_shared_ptr_is_null (*ptr)) {
      /* Release the old value of the pointer, possibly destroying it */
      mongoc_shared_ptr_release (ptr);
   }
   ptr->ptr = pointee;
   ptr->_aux = NULL;
   /* Take the new value */
   if (pointee != NULL) {
      assert (dtor != NULL);
      ptr->_aux = bson_malloc0 (sizeof (_mongoc_shared_ptr_aux));
      ptr->_aux->dtor = dtor;
      ptr->_aux->refcount = 1;
      ptr->_aux->managed = pointee;
   }
}

void
mongoc_shared_ptr_rebind (mongoc_shared_ptr *const out,
                          mongoc_shared_ptr const from)
{
   assert (out &&
           "NULL given as output argument to mongoc_shared_ptr_rebind()");
   mongoc_shared_ptr_release (out);
   *out = mongoc_shared_ptr_take (from);
}

mongoc_shared_ptr
mongoc_shared_ptr_create (void *pointee, void (*destroy) (void *))
{
   mongoc_shared_ptr ret = {0};
   mongoc_shared_ptr_rebind_raw (&ret, pointee, destroy);
   bson_once (&g_shared_ptr_mtx_init_once, _init_mtx);
   return ret;
}

void
mongoc_shared_ptr_rebind_atomic (mongoc_shared_ptr *const out,
                                 mongoc_shared_ptr const from)
{
   struct _mongoc_shared_ptr_aux *prev_aux = NULL;
   size_t prevcount = 0;
   assert (out &&
           "NULL given as output argument to mongoc_shared_ptr_rebind_atomic");

   {
      _shared_ptr_spin_lock ();
      prev_aux = out->_aux;
      if (prev_aux) {
         prevcount = bson_atomic_int_fetch_sub (
            &prev_aux->refcount, 1, bson_memory_order_relaxed);
      }
      *out = from;
      bson_atomic_int_fetch_add (
         &out->_aux->refcount, 1, bson_memory_order_relaxed);
      _shared_ptr_spin_unlock ();
   }

   if (prevcount == 1) {
      _release_aux (prev_aux);
   }
}

mongoc_shared_ptr
mongoc_shared_ptr_take (mongoc_shared_ptr const ptr)
{
   mongoc_shared_ptr ret = ptr;
   if (!mongoc_shared_ptr_is_null (ptr)) {
      bson_atomic_int_fetch_add (
         &ret._aux->refcount, 1, bson_memory_order_relaxed);
   }
   return ret;
}

mongoc_shared_ptr
mongoc_shared_ptr_take_atomic (mongoc_shared_ptr const *ptr)
{
   mongoc_shared_ptr r;
   assert (ptr && "NULL given to _mongoc_shared_ptr_take_atomic()");
   _shared_ptr_spin_lock ();
   r = mongoc_shared_ptr_take (*ptr);
   _shared_ptr_spin_unlock ();
   return r;
}

void
mongoc_shared_ptr_release (mongoc_shared_ptr *const ptr)
{
   assert (ptr && "NULL given to mongoc_shared_ptr_release()");
   assert (!mongoc_shared_ptr_is_null (*ptr) &&
           "Unbound mongoc_shared_ptr given to mongoc_shared_ptr_release");
   /* Decrement the reference count by one */
   size_t prevcount = bson_atomic_int_fetch_sub (
      &ptr->_aux->refcount, 1, bson_memory_order_relaxed);
   if (prevcount == 1) {
      /* We just decremented from one to zero, so this is the last instance.
       * Release the managed data. */
      _release_aux (ptr->_aux);
   }
   ptr->_aux = NULL;
   ptr->ptr = NULL;
}

int
mongoc_shared_ptr_refcount (mongoc_shared_ptr const ptr)
{
   assert (!mongoc_shared_ptr_is_null (ptr) &&
           "Unbound mongoc_shraed_ptr given to mongoc_shared_ptr_refcount");
   return (int) bson_atomic_int_fetch (&ptr._aux->refcount,
                                       bson_memory_order_relaxed);
}

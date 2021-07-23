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

#include "./mongoc-shared.h"

#include <bson/bson.h>

#include <assert.h>

typedef struct _mongoc_shared_aux {
   int refcount;
   void (*dtor) (void *);
   void *managed;
} _mongoc_shared_aux;

/** Get the pointed-to auxilary data for the given shared pointer */
static _mongoc_shared_aux *
_aux (const mongoc_shared_ptr p)
{
   return (_mongoc_shared_aux *) (p._aux);
}

/** Get the pointed-to auxilary data for the pointer to a shared pointer */
static _mongoc_shared_aux *
_paux (const mongoc_shared_ptr *ptrptr)
{
   return _aux (*ptrptr);
}

static void
_release_aux (_mongoc_shared_aux *aux)
{
   aux->dtor (aux->managed);
   bson_free (aux);
}

static int8_t g_shared_ptr_spin_mtx = 0;

static void
_shared_ptr_spin_lock ()
{
   while (true) {
      int8_t f = false;
      int8_t t = true;
      bool was_locked = bson_atomic_int8_compare_exchange (
         &g_shared_ptr_spin_mtx, f, t, bson_memorder_acquire);
      if (!was_locked) {
         return;
      }
   }
}

static void
_shared_ptr_spin_unlock ()
{
   bool was_locked = bson_atomic_int8_compare_exchange (
      &g_shared_ptr_spin_mtx, 1, 0, bson_memorder_release);
   assert (
      was_locked &&
      "_shared_ptr_spin_unlock() was called, but the spin lock was not held");
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
      ptr->_aux = bson_malloc0 (sizeof (_mongoc_shared_aux));
      _paux (ptr)->dtor = dtor;
      _paux (ptr)->refcount = 1;
      _paux (ptr)->managed = pointee;
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
   return ret;
}

void
mongoc_shared_ptr_rebind_atomic (mongoc_shared_ptr *const out,
                                 mongoc_shared_ptr const from)
{
   void *prev_aux = NULL;
   size_t prevcount = 0;
   assert (out &&
           "NULL given as output argument to mongoc_shared_ptr_rebind_atomic");

   {
      _shared_ptr_spin_lock ();
      prev_aux = out->_aux;
      if (prev_aux) {
         prevcount = bson_atomic_int_fetch_sub (
            &_paux (out)->refcount, 1, bson_memorder_seqcst);
      }
      *out = from;
      bson_atomic_int_fetch_add (
         &_paux (out)->refcount, 1, bson_memorder_seqcst);
      _shared_ptr_spin_unlock ();
   }

   if (prevcount == 1) {
      _release_aux ((_mongoc_shared_aux *) prev_aux);
   }
}

mongoc_shared_ptr
mongoc_shared_ptr_take (mongoc_shared_ptr const ptr)
{
   mongoc_shared_ptr ret = ptr;
   if (!mongoc_shared_ptr_is_null (ptr)) {
      bson_atomic_int_fetch_add (
         &_aux (ret)->refcount, 1, bson_memorder_seqcst);
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
      &_paux (ptr)->refcount, 1, bson_memorder_seqcst);
   if (prevcount == 1) {
      /* We just decremented from one to zero, so this is the last instance.
       * Release the managed data. */
      _release_aux (_paux (ptr));
   }
   ptr->_aux = NULL;
   ptr->ptr = NULL;
}

int
mongoc_shared_ptr_refcount (mongoc_shared_ptr const ptr)
{
   assert (!mongoc_shared_ptr_is_null (ptr) &&
           "Unbound mongoc_shraed_ptr given to mongoc_shared_ptr_refcount");
   return (int) bson_atomic_int_fetch (&_aux (ptr)->refcount,
                                       bson_memorder_relaxed);
}

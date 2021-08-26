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

#ifndef MONGOC_SHARED_H
#define MONGOC_SHARED_H

#include <stddef.h>

struct _mongoc_shared_ptr_aux;

/**
 * @brief A ref-counted thread-safe shared pointer to arbitrary data
 */
typedef struct mongoc_shared_ptr {
   /** Pointed-to data */
   void *ptr;
   /** Auxilary book-keeping. Do not touch. */
   struct _mongoc_shared_ptr_aux *_aux;
} mongoc_shared_ptr;

/**
 * @brief A "null" pointer constant for a mongoc_shared_ptr
 */
static const mongoc_shared_ptr MONGOC_SHARED_PTR_NULL = {NULL, NULL};

/**
 * @brief Rebind a shared pointer to the given memory resources
 *
 * @param ptr The shared pointer that will be rebound
 * @param pointee The pointer that we will point to. Should have been
 * dynamically allocated
 * @param destroy A destructor+deallocator for @param pointee, to be called when
 * the refcount reaches zero
 */
extern void
mongoc_shared_ptr_rebind_raw (mongoc_shared_ptr *ptr,
                              void *pointee,
                              void (*destroy) (void *));


/**
 * @brief Rebind the given shared pointer to have an equivalent value as
 * 'from'
 *
 * @param dest The shared pointer to rebind
 * @param from The shared pointer to take from
 */
extern void
mongoc_shared_ptr_rebind (mongoc_shared_ptr *dest,
                          const mongoc_shared_ptr from);

/**
 * @brief Rebind the given shared pointer to have an equivalent value as 'from'.
 * This atomic version is safe to call between threads when 'dest' may be access
 * simultaneously from another thread and at least one of those accesses is a
 * write.
 *
 * @param dest The shared pointer to rebind
 * @param from The shared pointer to take from
 */
extern void
mongoc_shared_ptr_rebind_atomic (mongoc_shared_ptr *dest,
                                 const mongoc_shared_ptr from);

/**
 * @brief Create a copy of the given shared pointer. Increases the reference
 * count on the object.
 *
 * @param ptr The pointer to copy from
 * @returns a new shared pointer that has the same pointee as @param ptr
 *
 * @note Must later call mongoc_shared_ptr_release() on the return value
 */
extern mongoc_shared_ptr
mongoc_shared_ptr_take (mongoc_shared_ptr const ptr);

/**
 * @brief Like @see _mongoc_shared_ptr_take, but is thread-safe in case @param
 * ptr might be written-to by another thread via
 * mongoc_shared_ptr_rebind_atomic()
 *
 * @note Must later call mongoc_shared_ptr_release() on the return value
 */
extern mongoc_shared_ptr
mongoc_shared_ptr_take_atomic (mongoc_shared_ptr const *ptr);

/**
 * @brief Release the ownership of the given shared pointer.
 *
 * If this causes the refcount to reach zero, then the destructor function will
 * be executed with the pointee. The ptr will be reset to NULL
 *
 * @param ptr The pointer to release and set to NULL
 *
 * @note This function is not thread safe if other threads may be
 * reading/writing to @param ptr simultaneously. To thread-safe release a shared
 * pointer, use mongoc_shared_ptr_rebind_atomic() with a null
 * _mongoc_shared_ptr as the 'from' argument
 */
extern void
mongoc_shared_ptr_release (mongoc_shared_ptr *ptr);

/**
 * @brief Obtain the number of hard references to the resource managed by the
 * given shared pointer. This should only be used for diagnostic and assertive
 * purposes.
 *
 * @param ptr A non-null shared pointer to check
 * @return int A positive integer reference count
 */
extern int
mongoc_shared_ptr_refcount (mongoc_shared_ptr ptr);

/**
 * @brief Check whether the given shared pointer is managing a resource.
 *
 * @note That the ptr.ptr MAY be NULL while the shared pointer is still managing
 * a resource.
 *
 * @return true If the pointer is managing a resource
 * @return false Otherwise
 */
static int
mongoc_shared_ptr_is_null (mongoc_shared_ptr ptr)
{
   return ptr._aux == 0;
}

/**
 * @brief Create a new shared pointer that manages the given memory, or NULL
 *
 * @param pointee The target of the pointer. Should be NULL or a dynamically
 * allocated data segment
 * @param destroy The destructor for the pointer. If @param pointee is non-NULL,
 * must be not NULL. This destructor will be called when the reference count
 * reaches zero. If should free the memory of @param pointee
 */
extern mongoc_shared_ptr
mongoc_shared_ptr_create (void *pointee, void (*destroy) (void *));

/** Get the managed pointer owned by the given shared pointer */
#define MONGOC_Shared_Pointee(Type, Pointer) ((Type *) ((Pointer).ptr))

/**
 * @brief Create a new shared pointer instance from 'Pointer' and bind a regular
 * pointer 'Type *VarName' in the calling scope.
 *
 * @param Type The pointed-to type of the shared pointer
 * @param VarName A name for a regular pointer to declare in the enclosing scope
 * @param Pointer A shared ppointer instance to take from
 */
#define MONGOC_Shared_Take(Type, VarName, Pointer) \
   mongoc_shared_ptr _shared_ptr_copy_##VarName =  \
      mongoc_shared_ptr_take (Pointer);            \
   Type *VarName = MONGOC_Shared_Pointee (Type, _shared_ptr_copy_##VarName)

/**
 * @brief Like @see MONGOC_Shared_Take, but thread-safe if another thread could
 * rebind @param Pointer concurrently.
 *
 * @param Type The pointed-to type of the shared pointer
 * @param VarName A name for a regular pointer to declare in the enclosing scope
 * @param Pointer A shared ppointer instance to take from
 */
#define MONGOC_Shared_Take_Atomic(Type, VarName, Pointer) \
   mongoc_shared_ptr _shared_ptr_copy_##VarName =         \
      mongoc_shared_ptr_take_atomic (Pointer);            \
   Type *VarName = MONGOC_Shared_Pointee (Type, _shared_ptr_copy_##VarName)

/**
 * @brief Release a shared pointer that was created with @see MONGOC_Shared_Take
 * or @see MONGOC_Shared_Take_Atomic.
 *
 * @param VarName The 'VarName' that was given when the pointer was taken
 */
#define MONGOC_Shared_Release(VarName)                         \
   do {                                                        \
      mongoc_shared_ptr_release (&_shared_ptr_copy_##VarName); \
      VarName = NULL;                                          \
   } while (0)

#endif /* MONGOC_SHARED_H */

/*
 * Copyright 2009-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef BSON_THREAD_BACKEND_H
#define BSON_THREAD_BACKEND_H

#include <bson/compat.h>
#include <bson/macros.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

BSON_BEGIN_DECLS

#ifdef _WIN32
typedef unsigned(__stdcall *bson_thread_backend_function_t)(void *);
#else
typedef void *(*bson_thread_backend_function_t)(void *);
#endif

/* All storage pointers refer to driver-owned objects whose representation and
 * size are private. Backends may key allocations by storage address; they must
 * not assume room for a pointer in that storage. Once storage has process
 * lifetime and its callback must handle concurrent calls. No callback may call
 * driver APIs. Thread storage holds a void * backend handle; join receives a
 * copy of that handle's storage. */
typedef struct bson_thread_backend_t {
   /* Set to sizeof(bson_thread_backend_t). Allows libraries with a newer
    * version of this structure to copy only the fields provided by callers. */
   size_t struct_size;
   void (*once)(void *storage, void (*callback)(void));
   int (*thread_create)(void *storage, bson_thread_backend_function_t callback, void *arg);
   int (*thread_join)(void *storage);
   void (*mutex_init)(void *storage);
   void (*mutex_destroy)(void *storage);
   void (*mutex_lock)(void *storage);
   void (*mutex_unlock)(void *storage);
   bool (*mutex_is_locked)(void *storage);
   void (*shared_mutex_init)(void *storage);
   void (*shared_mutex_destroy)(void *storage);
   void (*shared_mutex_lock)(void *storage);
   void (*shared_mutex_unlock)(void *storage);
   void (*shared_mutex_lock_shared)(void *storage);
   void (*shared_mutex_unlock_shared)(void *storage);
   void (*cond_init)(void *storage);
   void (*cond_destroy)(void *storage);
   int (*cond_wait)(void *cond, void *mutex);
   int (*cond_timedwait)(void *cond, void *mutex, int64_t timeout_ms);
   bool (*cond_is_timedout)(int result);
   void (*cond_signal)(void *storage);
   void (*cond_broadcast)(void *storage);
   void (*sleep)(int64_t usec);
} bson_thread_backend_t;

/* Use mongoc_set_thread_backend when using libmongoc; that function configures
 * both libraries. This setter is for standalone libbson users. Call before
 * any other libbson operation, from a single thread. NULL restores defaults. */
BSON_EXPORT(void)
bson_set_thread_backend(const bson_thread_backend_t *backend);

BSON_END_DECLS

#endif

#ifndef BSON_MEMORY_H_INCLUDED
#define BSON_MEMORY_H_INCLUDED

#include <bson/macros.h>


BSON_BEGIN_DECLS


typedef void *(BSON_CALL *bson_realloc_func) (void *mem, size_t num_bytes, void *ctx);

typedef struct _bson_mem_vtable_t {
   void *(BSON_CALL *malloc) (size_t num_bytes);
   void *(BSON_CALL *calloc) (size_t n_members, size_t num_bytes);
   void *(BSON_CALL *realloc) (void *mem, size_t num_bytes);
   void (BSON_CALL *free) (void *mem);
   void *(BSON_CALL *aligned_alloc) (size_t alignment, size_t num_bytes);
   void *padding[3];
} bson_mem_vtable_t;


BSON_EXPORT (void)
bson_mem_set_vtable (const bson_mem_vtable_t *vtable);
BSON_EXPORT (void)
bson_mem_restore_vtable (void);
BSON_EXPORT (void *)
bson_malloc (size_t num_bytes);
BSON_EXPORT (void *)
bson_malloc0 (size_t num_bytes);
BSON_EXPORT (void *)
bson_aligned_alloc (size_t alignment, size_t num_bytes);
BSON_EXPORT (void *)
bson_aligned_alloc0 (size_t alignment, size_t num_bytes);
BSON_EXPORT (void *)
bson_realloc (void *mem, size_t num_bytes);
BSON_EXPORT (void *)
bson_realloc_ctx (void *mem, size_t num_bytes, void *ctx);
BSON_EXPORT (void)
bson_free (void *mem);
BSON_EXPORT (void)
bson_zero_free (void *mem, size_t size);


#define BSON_ALIGNED_ALLOC(T) ((T *) (bson_aligned_alloc (BSON_ALIGNOF (T), sizeof (T))))
#define BSON_ALIGNED_ALLOC0(T) ((T *) (bson_aligned_alloc0 (BSON_ALIGNOF (T), sizeof (T))))

BSON_END_DECLS

#endif // BSON_MEMORY_H_INCLUDED

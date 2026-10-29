// Checks, using only the public libbson API, that a memory vtable installed with
// bson_mem_set_vtable() is honored by installed libbson artifacts, including by
// internal library code paths that do not call bson_malloc() and friends.
#include <bson/bson.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(Cond)                                                               \
   do {                                                                           \
      if (!(Cond)) {                                                              \
         fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #Cond); \
         exit(1);                                                                 \
      }                                                                           \
   } while (0)

// Counts the vtable callbacks that actually acquire or release memory.
static struct {
   size_t acquired;
   size_t released;
} counts;

static void *BSON_CALL
counting_malloc(size_t size)
{
   void *const p = malloc(size);
   counts.acquired += p != NULL;
   return p;
}

static void *BSON_CALL
counting_calloc(size_t n, size_t size)
{
   void *const p = calloc(n, size);
   counts.acquired += p != NULL;
   return p;
}

static void *BSON_CALL
counting_realloc(void *ptr, size_t size)
{
   void *const p = realloc(ptr, size);
   // Only a realloc from NULL acquires a new block
   counts.acquired += (p != NULL && ptr == NULL);
   return p;
}

static void BSON_CALL
counting_free(void *ptr)
{
   counts.released += ptr != NULL;
   free(ptr);
}

static void
install_counting_vtable(void)
{
   const bson_mem_vtable_t vtable = {
      .malloc = counting_malloc,
      .calloc = counting_calloc,
      .realloc = counting_realloc,
      .free = counting_free,
   };
   bson_mem_set_vtable(&vtable);
   memset(&counts, 0, sizeof counts);
}

// Build a document with `n` distinct keys, plus a duplicate of the first key if `dup`
static bson_t *
make_doc(int n, int dup)
{
   bson_t *const doc = bson_new();
   for (int i = 0; i < n; ++i) {
      char key[32];
      snprintf(key, sizeof key, "key-%d", i);
      CHECK(bson_append_int32(doc, key, -1, i));
   }
   if (dup) {
      CHECK(bson_append_int32(doc, "key-0", -1, 0));
   }
   return doc;
}

// Duplicate-key detection requires scratch storage that grows with the document.
// That storage is obtained inside libbson, and must be routed to the installed
// vtable and be fully released when validation returns.
static void
test_validate_scratch_storage(void)
{
   // Build the documents using the default allocator.
   bson_t *const unique = make_doc(500, 0);
   bson_t *const duplicated = make_doc(500, 1);

   install_counting_vtable();

   bson_error_t error;
   CHECK(bson_validate_with_error(unique, BSON_VALIDATE_DUPLICATE_KEYS, &error));
   CHECK(counts.acquired > 0);
   CHECK(counts.released == counts.acquired);

   memset(&counts, 0, sizeof counts);
   CHECK(!bson_validate_with_error(duplicated, BSON_VALIDATE_DUPLICATE_KEYS, &error));
   CHECK(error.code == BSON_VALIDATE_DUPLICATE_KEYS);
   CHECK(counts.acquired > 0);
   CHECK(counts.released == counts.acquired);

   // Without duplicate-key checking, no scratch storage is needed at all.
   memset(&counts, 0, sizeof counts);
   CHECK(bson_validate_with_error(duplicated, BSON_VALIDATE_NONE, &error));
   CHECK(counts.acquired == 0);
   CHECK(counts.released == 0);

   // Restoring the default vtable must stop all use of the installed one.
   bson_mem_restore_vtable();
   memset(&counts, 0, sizeof counts);
   CHECK(!bson_validate_with_error(duplicated, BSON_VALIDATE_DUPLICATE_KEYS, &error));
   CHECK(counts.acquired == 0);
   CHECK(counts.released == 0);

   bson_destroy(unique);
   bson_destroy(duplicated);
}

int
main(void)
{
   test_validate_scratch_storage();
   return 0;
}

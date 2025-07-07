#ifndef BSON_ERROR_T_INCLUDED
#define BSON_ERROR_T_INCLUDED

#include <bson/macros.h>

#include <stdint.h>

BSON_BEGIN_DECLS

#define BSON_ERROR_BUFFER_SIZE 503

BSON_ALIGNED_BEGIN (BSON_ALIGN_OF_PTR) // Aligned for backwards-compatibility.
typedef struct _bson_error_t {
   uint32_t domain;
   uint32_t code;
   char message[BSON_ERROR_BUFFER_SIZE];
   uint8_t reserved; // For internal use only!
} bson_error_t BSON_ALIGNED_END (BSON_ALIGN_OF_PTR);


BSON_STATIC_ASSERT2 (error_t, sizeof (bson_error_t) == 512);

#define BSON_ERROR_JSON 1
#define BSON_ERROR_READER 2
#define BSON_ERROR_INVALID 3
#define BSON_ERROR_VECTOR 4

BSON_EXPORT (void)
bson_set_error (bson_error_t *error, uint32_t domain, uint32_t code, const char *format, ...) BSON_GNUC_PRINTF (4, 5);

BSON_EXPORT (char *)
bson_strerror_r (int err_code, char *buf, size_t buflen);

BSON_END_DECLS


#endif // BSON_ERROR_T_INCLUDED

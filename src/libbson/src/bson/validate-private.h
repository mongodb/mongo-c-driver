#ifndef BSON_VALIDATE_PRIVATE_H_INCLUDED
#define BSON_VALIDATE_PRIVATE_H_INCLUDED

#include <bson/bson-types.h>

/**
 * @brief Private function backing the implementation of validation.
 *
 * Validation was previously defined in the overburdened `bson-iter.c`, but it
 * is now defined in its own file.
 *
 * @param bson The document to validate. Must be non-null.
 * @param flags Validation control flags
 * @param offset Receives the offset at which validation failed. Must be non-null.
 * @param error Receives the error describing why validation failed. Must be non-null.
 * @return true If the given document has no validation errors
 * @return false Otherwise
 */
extern bool
_bson_validate_impl_v2 (const bson_t *bson, bson_validate_flags_t flags, size_t *offset, bson_error_t *error);

#endif // BSON_VALIDATE_PRIVATE_H_INCLUDED

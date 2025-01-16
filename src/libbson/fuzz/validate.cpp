#include <stdint.h>

#include "validate.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
   return validate_one_input (data, size);
}

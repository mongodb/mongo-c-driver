#include <stdint.h>

#include "./fuzz_test_validate.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
   return validate_one_input (data, size);
}

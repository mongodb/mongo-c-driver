/*
Fuzz test `bson_utf8_escape_for_json`.

Configure using `-DMONGO_SANITIZE="address"` to enable the Address Sanitizer:

```sh
cmake \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DCMAKE_C_COMPILER="/opt/homebrew/opt/llvm/bin/clang" \
    -DMONGO_SANITIZE="address" \
    -S$(pwd) -B$(pwd)/cmake-build
```

Build with:
```sh
cmake --build cmake-build --target fuzz_test_bson_utf8_escape_for_json
```

Run with:
```sh
./cmake-build/src/libbson/fuzz_test_bson_utf8_escape_for_json
```

*/

#include <bson/bson.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    bool ok = bson_utf8_validate ((const char*) data, size, true /* allow null */);
    if (!ok) {
        // Ignore invalid UTF-8. Assume this is unexpected input.
        return 0;
    }

    char* got = bson_utf8_escape_for_json ((const char*) data, (ssize_t) size);
    BSON_ASSERT (got);
    bson_free (got);
    return 0;
}

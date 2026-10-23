/*
 * libFuzzer target for the config protocol parser (CI runs it with clang):
 *   clang -g -O1 -fsanitize=fuzzer,address,undefined -Iinclude -Itests \
 *     fuzz/fuzz_protocol.c src/*.c -o fuzz_protocol
 *   ./fuzz_protocol -max_total_time=60 fuzz/corpus
 * Any broken invariant aborts, and libFuzzer saves the input.
 */
#include <stdlib.h>

#include "fuzz_protocol.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (!fuzz_protocol_one(data, size)) {
        abort();
    }
    return 0;
}

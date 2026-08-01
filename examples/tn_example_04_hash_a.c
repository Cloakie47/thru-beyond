/* Example 04, arm A: portable C SHA-256, no intrinsics. */

#include <stdint.h>

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

#define SHA256_SSIG0(x) (ROR32((x), 7) ^ ROR32((x), 18) ^ ((x) >> 3))
#define SHA256_SSIG1(x) (ROR32((x), 17) ^ ROR32((x), 19) ^ ((x) >> 10))
#define SHA256_BSIG0(x) (ROR32((x), 2) ^ ROR32((x), 13) ^ ROR32((x), 22))
#define SHA256_BSIG1(x) (ROR32((x), 6) ^ ROR32((x), 11) ^ ROR32((x), 25))

#include "tn_example_04_sha256_impl.h"

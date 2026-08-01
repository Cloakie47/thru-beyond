/* Example 04, arm B: SHA-256 using the RISC-V Zknh scalar-crypto sigma
   instructions (sha256sig0/sig1/sum0/sum1). Uses the GCC builtins when the
   compiler provides them, otherwise falls back to inline asm emitting the
   same single instruction — the SDK's -march enables zknh either way. */

#include <stdint.h>

#if defined(__has_builtin) && __has_builtin(__builtin_riscv_sha256sig0)

#define SHA256_SSIG0(x) __builtin_riscv_sha256sig0(x)
#define SHA256_SSIG1(x) __builtin_riscv_sha256sig1(x)
#define SHA256_BSIG0(x) __builtin_riscv_sha256sum0(x)
#define SHA256_BSIG1(x) __builtin_riscv_sha256sum1(x)

#else

static inline uint32_t ex04_zknh_sig0(uint32_t x) {
    unsigned long r;
    __asm__("sha256sig0 %0, %1" : "=r"(r) : "r"((unsigned long)x));
    return (uint32_t)r;
}
static inline uint32_t ex04_zknh_sig1(uint32_t x) {
    unsigned long r;
    __asm__("sha256sig1 %0, %1" : "=r"(r) : "r"((unsigned long)x));
    return (uint32_t)r;
}
static inline uint32_t ex04_zknh_sum0(uint32_t x) {
    unsigned long r;
    __asm__("sha256sum0 %0, %1" : "=r"(r) : "r"((unsigned long)x));
    return (uint32_t)r;
}
static inline uint32_t ex04_zknh_sum1(uint32_t x) {
    unsigned long r;
    __asm__("sha256sum1 %0, %1" : "=r"(r) : "r"((unsigned long)x));
    return (uint32_t)r;
}

#define SHA256_SSIG0(x) ex04_zknh_sig0(x)
#define SHA256_SSIG1(x) ex04_zknh_sig1(x)
#define SHA256_BSIG0(x) ex04_zknh_sum0(x)
#define SHA256_BSIG1(x) ex04_zknh_sum1(x)

#endif

#include "tn_example_04_sha256_impl.h"

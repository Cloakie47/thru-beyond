#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 06: measure the instruction term directly. Every earlier example
   computed instruction cost as a residual; here the loop bodies are inline
   asm so the executed encodings are exact and everything else — pages,
   syscalls, data — is held constant. One binary, dispatched by type. */

#define EX06_ERR_BAD_SIZE (0x6000UL)
#define EX06_ERR_BAD_TYPE (0x6001UL)

#define EX06_INSTR_SPIN       (0U)
#define EX06_INSTR_SPIN_WIDE  (1U)
#define EX06_INSTR_SPIN_LOAD  (2U)
#define EX06_INSTR_SPIN_STORE (3U)
#define EX06_INSTR_LOG0       (4U)
#define EX06_INSTR_LOG8       (5U)
#define EX06_INSTR_GROW       (6U)

#define EX06_ERR_GROW     (0x6002UL)
#define EX06_STACK_TOP    ((0x05UL << 40) | (0x0001UL << 24))

typedef struct __attribute__((packed)) {
    uint instruction_type;
    uint n;
} ex06_args_t;

/* Body: 4 ALU/branch instructions, compressed where the assembler can. */
static ulong ex06_spin(ulong n) {
    ulong acc = 1;
    __asm__ volatile(
        "beqz %[n], 2f\n"
        "1:\n\t"
        "addi %[acc], %[acc], 3\n\t"
        "add  %[acc], %[acc], %[n]\n\t"
        "addi %[n], %[n], -1\n\t"
        "bnez %[n], 1b\n"
        "2:"
        : [acc] "+r"(acc), [n] "+r"(n));
    return acc;
}

/* Identical body, full-width encodings only. */
static ulong ex06_spin_wide(ulong n) {
    ulong acc = 1;
    __asm__ volatile(
        ".option push\n"
        ".option norvc\n"
        "beqz %[n], 2f\n"
        "1:\n\t"
        "addi %[acc], %[acc], 3\n\t"
        "add  %[acc], %[acc], %[n]\n\t"
        "addi %[n], %[n], -1\n\t"
        "bnez %[n], 1b\n"
        "2:\n"
        ".option pop"
        : [acc] "+r"(acc), [n] "+r"(n));
    return acc;
}

/* Body adds one 1-byte load from the read-only txn-data segment. */
static ulong ex06_spin_load(ulong n, uchar const *p) {
    ulong acc = 1, t;
    __asm__ volatile(
        "beqz %[n], 2f\n"
        "1:\n\t"
        "lbu  %[t], 0(%[p])\n\t"
        "add  %[acc], %[acc], %[t]\n\t"
        "addi %[n], %[n], -1\n\t"
        "bnez %[n], 1b\n"
        "2:"
        : [acc] "+r"(acc), [n] "+r"(n), [t] "=&r"(t)
        : [p] "r"(p));
    return acc;
}

/* Body adds one 1-byte store to the already-mapped stack page. */
static ulong ex06_spin_store(ulong n, volatile uchar *p) {
    ulong acc = 1;
    __asm__ volatile(
        "beqz %[n], 2f\n"
        "1:\n\t"
        "sb   %[n], 0(%[p])\n\t"
        "add  %[acc], %[acc], %[n]\n\t"
        "addi %[n], %[n], -1\n\t"
        "bnez %[n], 1b\n"
        "2:"
        : [acc] "+r"(acc), [n] "+r"(n)
        : [p] "r"(p)
        : "memory");
    return acc;
}

TSDK_ENTRYPOINT_FN void start(void) {
    tsdk_txn_t const *txn = tsdk_get_txn();
    uchar const *instruction_data = tsdk_txn_get_instr_data(txn);
    ulong instruction_data_sz = tsdk_txn_get_instr_data_sz(txn);

    if (instruction_data_sz != sizeof(ex06_args_t)) {
        tsdk_revert(EX06_ERR_BAD_SIZE);
    }
    ex06_args_t const *args = (ex06_args_t const *)instruction_data;
    ulong n = (ulong)args->n;
    ulong acc = 0;
    uchar sink = 0;

    switch (args->instruction_type) {
        case EX06_INSTR_SPIN:
            acc = ex06_spin(n);
            break;
        case EX06_INSTR_SPIN_WIDE:
            acc = ex06_spin_wide(n);
            break;
        case EX06_INSTR_SPIN_LOAD:
            acc = ex06_spin_load(n, instruction_data);
            break;
        case EX06_INSTR_SPIN_STORE:
            acc = ex06_spin_store(n, &sink);
            break;
        case EX06_INSTR_LOG0:
            tsys_log(instruction_data, 0);
            break;
        case EX06_INSTR_LOG8:
            tsys_log(instruction_data, 8);
            break;
        case EX06_INSTR_GROW:
            /* Grow the stack segment by n bytes past the entry stub's one
               page. Discriminates per-byte vs per-page allocation charging. */
            if (tsys_set_anonymous_segment_sz(
                    (void *)(EX06_STACK_TOP - 4096UL - n)) != TSDK_SUCCESS) {
                tsdk_revert(EX06_ERR_GROW);
            }
            break;
        default:
            tsdk_revert(EX06_ERR_BAD_TYPE);
    }
    /* keep acc live without memory traffic */
    __asm__ volatile("" :: "r"(acc));
    tsdk_return(TSDK_SUCCESS);
}

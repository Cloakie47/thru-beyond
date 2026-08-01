#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 07 caller: prices cross-program invocation. One binary,
   dispatched by type; the callee's transaction account index comes from
   instruction data so the code path never depends on account layout. */

#define EX07_ERR_BAD_SIZE (0x7100UL)
#define EX07_ERR_BAD_TYPE (0x7101UL)
#define EX07_ERR_INVOKE   (0x7200UL)

#define EX07_INSTR_NO_CPI     (0U)
#define EX07_INSTR_CPI_1      (1U)
#define EX07_INSTR_CPI_N      (2U)
#define EX07_INSTR_CPI_DEEP_N (3U)
#define EX07_INSTR_CPI_REVERT (4U)
#define EX07_INSTR_CPI_FOREIGN (5U)

typedef struct __attribute__((packed)) {
    uint   instruction_type;
    uint   n;
    ushort callee_idx;
    ushort aux_idx;
} ex07_caller_args_t;

typedef struct __attribute__((packed)) {
    uint   op;
    uint   depth;
    ushort self_idx;
} ex07_callee_args_t;

static void ex07_invoke_callee(uint op, uint depth, ushort callee_idx) {
    ex07_callee_args_t a;
    a.op = op;
    a.depth = depth;
    a.self_idx = callee_idx;
    ulong invoke_err = 0UL;
    ulong r = tsys_invoke(&a, sizeof(a), callee_idx, NULL, &invoke_err);
    if (r != TSDK_SUCCESS) {
        tsdk_revert(EX07_ERR_INVOKE + r);
    }
    if (invoke_err != TSDK_SUCCESS) {
        tsdk_revert(invoke_err);
    }
}

/* Instruction data via entrypoint registers (see callee comment) — works
   for top-level execution too, since the VM initializes a0/a1 the same way. */
TSDK_ENTRYPOINT_FN void start(uchar const *instruction_data,
                              ulong instruction_data_sz) {
    if (instruction_data_sz != sizeof(ex07_caller_args_t)) {
        tsdk_revert(EX07_ERR_BAD_SIZE);
    }
    ex07_caller_args_t const *args =
        (ex07_caller_args_t const *)instruction_data;
    uint n = args->n;
    ushort callee_idx = args->callee_idx;

    switch (args->instruction_type) {
        case EX07_INSTR_NO_CPI: {
            /* the callee's op-0 work, inline: read a u64-sized value, return */
            volatile uint sink = n;
            (void)sink;
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case EX07_INSTR_CPI_1:
            ex07_invoke_callee(0U, 0U, callee_idx);
            tsdk_return(TSDK_SUCCESS);
            break;
        case EX07_INSTR_CPI_N:
            for (uint i = 0; i < n; i++) {
                ex07_invoke_callee(0U, 0U, callee_idx);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        case EX07_INSTR_CPI_DEEP_N:
            ex07_invoke_callee(2U, n, callee_idx);
            tsdk_return(TSDK_SUCCESS);
            break;
        case EX07_INSTR_CPI_REVERT: {
            /* absorb the callee's failure; report both codes via an event
               so the transaction itself succeeds and CU is observable */
            ex07_callee_args_t a;
            a.op = 1U;
            a.depth = 0U;
            a.self_idx = callee_idx;
            ulong invoke_err = 0UL;
            ulong r = tsys_invoke(&a, sizeof(a), callee_idx, NULL,
                                  &invoke_err);
            ulong codes[2];
            codes[0] = r;
            codes[1] = invoke_err;
            tsys_emit_event(codes, sizeof(codes));
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case EX07_INSTR_CPI_FOREIGN: {
            /* account-index identity probe: callee op 3 reads the u64 at
               offset 0 of the account at aux_idx and emits it. (The original
               plan — invoking ex02's increment_e — is impossible: ex02 reads
               instruction data via the txn accessors, which under CPI see
               the caller's top-level data. Documented in the README.) */
            ex07_invoke_callee(3U, (uint)args->aux_idx, callee_idx);
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        default:
            tsdk_revert(EX07_ERR_BAD_TYPE);
    }
    tsdk_revert(EX07_ERR_BAD_TYPE);
}

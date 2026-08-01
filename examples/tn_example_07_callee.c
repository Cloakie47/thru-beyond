#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 07 callee: deliberately tiny. CPI-CRITICAL DETAIL: instruction
   data reaches an invoked program via the entrypoint's registers (a0 =
   pointer, a1 = size — entrypoint.S saves a0 and passes it to start), NOT
   via tsdk_txn_get_instr_data(), which always reads the TOP-LEVEL
   transaction. A first draft used the txn accessors and rejected every
   CPI with its own size check. Ops: 0 return, 1 revert, 2 recurse to a
   given depth, 3 read the u64 at offset 0 of the account at `depth` and
   emit it (account-index identity probe). */

#define EX07C_ERR_BAD_SIZE (0x7000UL)
#define EX07C_ERR_BAD_OP   (0x7001UL)
#define EX07C_ERR_DATA_PTR (0x7002UL)
#define EX07C_ERR_REVERT   (0x7BADUL)
#define EX07C_ERR_INVOKE   (0x7C00UL)

typedef struct __attribute__((packed)) {
    uint   op;
    uint   depth;
    ushort self_idx;
} ex07_callee_args_t;

TSDK_ENTRYPOINT_FN void start(uchar const *instruction_data,
                              ulong instruction_data_sz) {
    if (instruction_data_sz != sizeof(ex07_callee_args_t)) {
        tsdk_revert(EX07C_ERR_BAD_SIZE);
    }
    ex07_callee_args_t const *args =
        (ex07_callee_args_t const *)instruction_data;

    if (args->op == 0U) {
        tsdk_return(TSDK_SUCCESS);
    } else if (args->op == 1U) {
        tsdk_revert(EX07C_ERR_REVERT);
    } else if (args->op == 2U) {
        if (args->depth == 0U) {
            tsdk_return(TSDK_SUCCESS);
        }
        ex07_callee_args_t next;
        next.op = 2U;
        next.depth = args->depth - 1U;
        next.self_idx = args->self_idx;
        ulong invoke_err = 0UL;
        ulong r = tsys_invoke(&next, sizeof(next), args->self_idx,
                              NULL, &invoke_err);
        if (r != TSDK_SUCCESS) {
            tsdk_revert(EX07C_ERR_INVOKE + r);
        }
        if (invoke_err != TSDK_SUCCESS) {
            tsdk_revert(invoke_err);
        }
        tsdk_return(TSDK_SUCCESS);
    } else if (args->op == 3U) {
        ulong const *p =
            (ulong const *)tsdk_get_account_data_ptr((ushort)args->depth);
        if (p == NULL) {
            tsdk_revert(EX07C_ERR_DATA_PTR);
        }
        ulong v = *p;
        tsys_emit_event(&v, sizeof(v));
        tsdk_return(TSDK_SUCCESS);
    }
    tsdk_revert(EX07C_ERR_BAD_OP);
}

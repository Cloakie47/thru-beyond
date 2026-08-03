#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 02: the account lifecycle — create, increment, resize, delete —
   one binary, dispatched by instruction type, so all comparisons are
   intra-binary. */

#define EX02_ERR_BAD_SIZE  (0x2000UL)
#define EX02_ERR_BAD_TYPE  (0x2001UL)
#define EX02_ERR_CREATE    (0x2002UL)
#define EX02_ERR_WRITABLE  (0x2003UL)
#define EX02_ERR_RESIZE    (0x2004UL)
#define EX02_ERR_DATA_PTR  (0x2005UL)
#define EX02_ERR_DELETE    (0x2006UL)

#define EX02_INSTR_CREATE      (0U)
#define EX02_INSTR_INCREMENT   (1U)
#define EX02_INSTR_INCREMENT_E (2U)
#define EX02_INSTR_RESIZE_TO   (3U)
#define EX02_INSTR_DELETE      (4U)
#define EX02_INSTR_WRITE_AT    (5U)
#define EX02_INSTR_WRITE_TWO   (6U)
#define EX02_INSTR_PAY_OUT     (7U)

typedef struct __attribute__((packed)) {
    uint   instruction_type;
    ushort account_index;
    uchar  seed[TN_SEED_SIZE];
    uint   proof_size;
} ex02_create_args_t;

typedef struct __attribute__((packed)) {
    uint   instruction_type;
    ushort account_index;
} ex02_op_args_t;

typedef struct __attribute__((packed)) {
    uint   instruction_type;
    ushort account_index;
    uint   new_size;
} ex02_resize_args_t;

static void handle_create(uchar const *instruction_data) {
    ex02_create_args_t const *args =
        (ex02_create_args_t const *)instruction_data;

    uchar const *proof = NULL;
    if (args->proof_size > 0) {
        proof = instruction_data + sizeof(ex02_create_args_t);
    }
    if (tsys_account_create(args->account_index, args->seed,
                            proof, args->proof_size) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_CREATE);
    }
    if (tsys_set_account_data_writable(args->account_index) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_WRITABLE);
    }
    if (tsys_account_resize(args->account_index, sizeof(ulong)) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_RESIZE);
    }
    ulong *counter = (ulong *)tsdk_get_account_data_ptr(args->account_index);
    if (counter == NULL) {
        tsdk_revert(EX02_ERR_DATA_PTR);
    }
    *counter = 0UL;
    tsdk_return(TSDK_SUCCESS);
}

static void handle_increment(ushort idx, uint emit) {
    ulong *counter = (ulong *)tsdk_get_account_data_ptr(idx);
    if (counter == NULL) {
        tsdk_revert(EX02_ERR_DATA_PTR);
    }
    if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_WRITABLE);
    }
    ulong v = *counter + 1UL;
    *counter = v;
    if (emit) {
        tsys_emit_event(&v, sizeof(ulong));
    }
    tsdk_return(TSDK_SUCCESS);
}

static void handle_resize_to(ushort idx, uint new_size) {
    if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_WRITABLE);
    }
    if (tsys_account_resize(idx, new_size) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_RESIZE);
    }
    tsdk_return(TSDK_SUCCESS);
}

/* Write one byte at an arbitrary offset (or at 0 and the offset) — probes
   what a CoW copy of a partially filled trailing page charges. The offset
   comes from instruction data, so the code path is identical for every
   offset value. */
static void handle_write_at(ushort idx, uint offset, uint also_zero) {
    volatile uchar *p = (volatile uchar *)tsdk_get_account_data_ptr(idx);
    if (p == NULL) {
        tsdk_revert(EX02_ERR_DATA_PTR);
    }
    if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_WRITABLE);
    }
    if (also_zero) {
        p[0] = 1;
    }
    p[offset] = 1;
    tsdk_return(TSDK_SUCCESS);
}

static void handle_delete(ushort idx) {
    if (tsys_account_delete(idx, NULL) != TSDK_SUCCESS) {
        tsdk_revert(EX02_ERR_DELETE);
    }
    tsdk_return(TSDK_SUCCESS);
}

TSDK_ENTRYPOINT_FN void start(void) {
    tsdk_txn_t const *txn = tsdk_get_txn();
    uchar const *instruction_data = tsdk_txn_get_instr_data(txn);
    ulong instruction_data_sz = tsdk_txn_get_instr_data_sz(txn);

    if (instruction_data_sz < sizeof(uint)) {
        tsdk_revert(EX02_ERR_BAD_SIZE);
    }
    uint const *instruction_type = (uint const *)instruction_data;

    switch (*instruction_type) {
        case EX02_INSTR_CREATE: {
            if (instruction_data_sz < sizeof(ex02_create_args_t)) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            ex02_create_args_t const *args =
                (ex02_create_args_t const *)instruction_data;
            if (instruction_data_sz !=
                    sizeof(ex02_create_args_t) + args->proof_size) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            handle_create(instruction_data);
            break;
        }
        case EX02_INSTR_INCREMENT:
        case EX02_INSTR_INCREMENT_E: {
            if (instruction_data_sz != sizeof(ex02_op_args_t)) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            ex02_op_args_t const *args =
                (ex02_op_args_t const *)instruction_data;
            handle_increment(args->account_index,
                             *instruction_type == EX02_INSTR_INCREMENT_E);
            break;
        }
        case EX02_INSTR_RESIZE_TO: {
            if (instruction_data_sz != sizeof(ex02_resize_args_t)) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            ex02_resize_args_t const *args =
                (ex02_resize_args_t const *)instruction_data;
            handle_resize_to(args->account_index, args->new_size);
            break;
        }
        case EX02_INSTR_WRITE_AT:
        case EX02_INSTR_WRITE_TWO: {
            if (instruction_data_sz != sizeof(ex02_resize_args_t)) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            ex02_resize_args_t const *args =
                (ex02_resize_args_t const *)instruction_data;
            handle_write_at(args->account_index, args->new_size,
                            *instruction_type == EX02_INSTR_WRITE_TWO);
            break;
        }
        case EX02_INSTR_PAY_OUT: {
            /* transfer <u32 amount> from the program-owned account at
               args.account_index to the fee payer (idx 0) — exercises
               tsys_account_transfer with proper (owner) authorization */
            if (instruction_data_sz != sizeof(ex02_resize_args_t)) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            ex02_resize_args_t const *args =
                (ex02_resize_args_t const *)instruction_data;
            ulong r = tsys_account_transfer((ulong)args->account_index, 0UL,
                                            (ulong)args->new_size);
            if (r != TSDK_SUCCESS) {
                tsdk_revert(0x2100UL + r);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case EX02_INSTR_DELETE: {
            if (instruction_data_sz != sizeof(ex02_op_args_t)) {
                tsdk_revert(EX02_ERR_BAD_SIZE);
            }
            ex02_op_args_t const *args =
                (ex02_op_args_t const *)instruction_data;
            handle_delete(args->account_index);
            break;
        }
        default:
            tsdk_revert(EX02_ERR_BAD_TYPE);
    }
    tsdk_revert(EX02_ERR_BAD_TYPE);
}

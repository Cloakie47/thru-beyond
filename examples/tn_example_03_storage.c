#include <stddef.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 03: controlled experiment on page-fault cost.
   One 8192-byte account (two 4KB pages). The write handlers are identical
   except for which offsets they touch, so CU deltas isolate the cost of
   touching a second page. Accesses are single bytes at fixed offsets —
   a single access must never span a 4KB page boundary. */

#define EX03_ERR_BAD_SIZE  (0x3000UL)
#define EX03_ERR_BAD_TYPE  (0x3001UL)
#define EX03_ERR_CREATE    (0x3002UL)
#define EX03_ERR_WRITABLE  (0x3003UL)
#define EX03_ERR_RESIZE    (0x3004UL)
#define EX03_ERR_DATA_PTR  (0x3005UL)

#define EX03_ACCOUNT_SIZE  (8192UL)

#define EX03_INSTR_INIT        (0U)
#define EX03_INSTR_WRITE_P0    (1U)
#define EX03_INSTR_WRITE_P0_X2 (2U)
#define EX03_INSTR_WRITE_P0_P1 (3U)
#define EX03_INSTR_READ_P1     (4U)

typedef struct __attribute__((packed)) {
    uint   instruction_type;
    ushort account_index;
    uchar  seed[TN_SEED_SIZE];
    uint   proof_size;
} ex03_init_args_t;

typedef struct __attribute__((packed)) {
    uint   instruction_type;
    ushort account_index;
} ex03_op_args_t;

static void handle_init(uchar const *instruction_data) {
    ex03_init_args_t const *args = (ex03_init_args_t const *)instruction_data;

    uchar const *proof = NULL;
    if (args->proof_size > 0) {
        proof = instruction_data + sizeof(ex03_init_args_t);
    }

    if (tsys_account_create(args->account_index, args->seed,
                            proof, args->proof_size) != TSDK_SUCCESS) {
        tsdk_revert(EX03_ERR_CREATE);
    }
    if (tsys_set_account_data_writable(args->account_index) != TSDK_SUCCESS) {
        tsdk_revert(EX03_ERR_WRITABLE);
    }
    if (tsys_account_resize(args->account_index, EX03_ACCOUNT_SIZE) != TSDK_SUCCESS) {
        tsdk_revert(EX03_ERR_RESIZE);
    }
    tsdk_return(TSDK_SUCCESS);
}

/* Write handlers: identical structure, one tsys_set_account_data_writable
   call each (its 512 CU cancels in every delta), volatile single-byte
   stores so the compiler cannot merge or elide them. */

static void handle_write_p0(ushort idx) {
    volatile uchar *d = (volatile uchar *)tsdk_get_account_data_ptr(idx);
    if (d == NULL) {
        tsdk_revert(EX03_ERR_DATA_PTR);
    }
    if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
        tsdk_revert(EX03_ERR_WRITABLE);
    }
    d[0] = 1;
    tsdk_return(TSDK_SUCCESS);
}

static void handle_write_p0_x2(ushort idx) {
    volatile uchar *d = (volatile uchar *)tsdk_get_account_data_ptr(idx);
    if (d == NULL) {
        tsdk_revert(EX03_ERR_DATA_PTR);
    }
    if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
        tsdk_revert(EX03_ERR_WRITABLE);
    }
    d[0] = 1;
    d[100] = 1;   /* second write, same page */
    tsdk_return(TSDK_SUCCESS);
}

static void handle_write_p0_p1(ushort idx) {
    volatile uchar *d = (volatile uchar *)tsdk_get_account_data_ptr(idx);
    if (d == NULL) {
        tsdk_revert(EX03_ERR_DATA_PTR);
    }
    if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
        tsdk_revert(EX03_ERR_WRITABLE);
    }
    d[0] = 1;
    d[4096] = 1;  /* second write, second page */
    tsdk_return(TSDK_SUCCESS);
}

static void handle_read_p1(ushort idx) {
    volatile uchar const *d =
        (volatile uchar const *)tsdk_get_account_data_ptr(idx);
    if (d == NULL) {
        tsdk_revert(EX03_ERR_DATA_PTR);
    }
    uchar v = d[4096];  /* read second page, no writable call, no store */
    (void)v;
    tsdk_return(TSDK_SUCCESS);
}

TSDK_ENTRYPOINT_FN void start(void) {
    tsdk_txn_t const *txn = tsdk_get_txn();
    uchar const *instruction_data = tsdk_txn_get_instr_data(txn);
    ulong instruction_data_sz = tsdk_txn_get_instr_data_sz(txn);

    if (instruction_data_sz < sizeof(uint)) {
        tsdk_revert(EX03_ERR_BAD_SIZE);
    }

    uint const *instruction_type = (uint const *)instruction_data;

    switch (*instruction_type) {
        case EX03_INSTR_INIT: {
            if (instruction_data_sz < sizeof(ex03_init_args_t)) {
                tsdk_revert(EX03_ERR_BAD_SIZE);
            }
            ex03_init_args_t const *args =
                (ex03_init_args_t const *)instruction_data;
            if (instruction_data_sz !=
                    sizeof(ex03_init_args_t) + args->proof_size) {
                tsdk_revert(EX03_ERR_BAD_SIZE);
            }
            handle_init(instruction_data);
            break;
        }
        case EX03_INSTR_WRITE_P0:
        case EX03_INSTR_WRITE_P0_X2:
        case EX03_INSTR_WRITE_P0_P1:
        case EX03_INSTR_READ_P1: {
            if (instruction_data_sz != sizeof(ex03_op_args_t)) {
                tsdk_revert(EX03_ERR_BAD_SIZE);
            }
            ex03_op_args_t const *args =
                (ex03_op_args_t const *)instruction_data;
            if (*instruction_type == EX03_INSTR_WRITE_P0) {
                handle_write_p0(args->account_index);
            } else if (*instruction_type == EX03_INSTR_WRITE_P0_X2) {
                handle_write_p0_x2(args->account_index);
            } else if (*instruction_type == EX03_INSTR_WRITE_P0_P1) {
                handle_write_p0_p1(args->account_index);
            } else {
                handle_read_p1(args->account_index);
            }
            break;
        }
        default:
            tsdk_revert(EX03_ERR_BAD_TYPE);
    }

    tsdk_revert(EX03_ERR_BAD_TYPE);
}

#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 05: does the event/anonymous page cost 4,096 CU or not?
   All variants live in ONE binary dispatched by instruction type, so codegen
   is identical across comparisons.

   Memory layout lessons that shaped this design (both discovered the hard way):
   - The SDK entry stub maps exactly ONE 4KB stack page before calling start()
     (entrypoint.S: set_anonymous_segment_sz(sp - 4096)). Bigger frames fault.
     Every instruction here therefore grows the stack to a constant 8 pages as
     its first action — the same syscall on every path, so it cancels in deltas.
   - The program image (including .data/.bss) is mapped in the segment type
     named TSDK_SEG_TYPE_READONLY_DATA. Writes to static arrays fault: Thru C
     programs have NO writable globals. Buffers live on the (grown) stack.

   The fill buffer is the same maximum size regardless of how many bytes are
   emitted — only the emitted length varies, so allocation cost is constant. */

#define EX05_ERR_BAD_SIZE  (0x5000UL)
#define EX05_ERR_BAD_N     (0x5001UL)
#define EX05_ERR_BAD_PAGES (0x5002UL)
#define EX05_ERR_BAD_TYPE  (0x5003UL)
#define EX05_ERR_GROW      (0x5004UL)

#define EX05_BUF_MAX     (8256U)
#define EX05_ARR_PAGES   (4U)
#define EX05_STACK_TOP   ((0x05UL << 40) | (0x0001UL << 24))
#define EX05_STACK_PAGES (8UL)

#define EX05_INSTR_RETURN_ONLY (0U)
#define EX05_INSTR_EMIT_N      (1U)
#define EX05_INSTR_FILL_ONLY   (2U)
#define EX05_INSTR_TOUCH_STACK (3U)

typedef struct __attribute__((packed)) {
    uint instruction_type;
    uint n;
} ex05_args_t;

/* noinline: the fill loop is the same machine code for emit_n and fill_only. */
static __attribute__((noinline)) void ex05_do_fill(volatile uchar *b, uint n) {
    for (uint i = 0; i < n; i++) {
        b[i] = (uchar)i;
    }
}

static __attribute__((noinline, noreturn)) void ex05_emit_n(uint n) {
    uchar buf[EX05_BUF_MAX];
    ex05_do_fill(buf, n);
    tsys_emit_event(buf, n);
    tsdk_return(TSDK_SUCCESS);
}

static __attribute__((noinline, noreturn)) void ex05_fill_only(uint n) {
    uchar buf[EX05_BUF_MAX];
    ex05_do_fill(buf, n);
    tsdk_return(TSDK_SUCCESS);
}

/* One byte written per 4KB stack page; stride 4096 guarantees each
   single-byte store lands on a distinct page, never spanning a boundary. */
static __attribute__((noinline, noreturn)) void ex05_touch_stack(uint pages) {
    volatile uchar arr[EX05_ARR_PAGES * 4096U];
    for (uint p = 0; p < pages; p++) {
        arr[(ulong)p * 4096UL] = 1;
    }
    uchar v = arr[0];  /* constant single read-back; cancels in P deltas */
    (void)v;
    tsdk_return(TSDK_SUCCESS);
}

TSDK_ENTRYPOINT_FN void start(void) {
    /* Constant on every path, including the baseline: grow the stack segment
       from the entry stub's 1 page to 8 pages (mapping only — no page is
       touched by the grow itself). */
    if (tsys_set_anonymous_segment_sz(
            (void *)(EX05_STACK_TOP - EX05_STACK_PAGES * 4096UL)) != TSDK_SUCCESS) {
        tsdk_revert(EX05_ERR_GROW);
    }

    tsdk_txn_t const *txn = tsdk_get_txn();
    uchar const *instruction_data = tsdk_txn_get_instr_data(txn);
    ulong instruction_data_sz = tsdk_txn_get_instr_data_sz(txn);

    if (instruction_data_sz != sizeof(ex05_args_t)) {
        tsdk_revert(EX05_ERR_BAD_SIZE);
    }
    ex05_args_t const *args = (ex05_args_t const *)instruction_data;
    uint n = args->n;

    switch (args->instruction_type) {
        case EX05_INSTR_RETURN_ONLY:
            tsdk_return(TSDK_SUCCESS);
            break;
        case EX05_INSTR_EMIT_N:
            if (n > EX05_BUF_MAX) {
                tsdk_revert(EX05_ERR_BAD_N);
            }
            ex05_emit_n(n);
            break;
        case EX05_INSTR_FILL_ONLY:
            if (n > EX05_BUF_MAX) {
                tsdk_revert(EX05_ERR_BAD_N);
            }
            ex05_fill_only(n);
            break;
        case EX05_INSTR_TOUCH_STACK:
            if (n == 0 || n > EX05_ARR_PAGES) {
                tsdk_revert(EX05_ERR_BAD_PAGES);
            }
            ex05_touch_stack(n);
            break;
        default:
            tsdk_revert(EX05_ERR_BAD_TYPE);
    }
    tsdk_revert(EX05_ERR_BAD_TYPE);
}

#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 10: the rolling block-history window. The last 512 blocks are
   mapped read-only at segment (type 0x00, idx 0x0004), one 0x1000-spaced
   record per block, blocks_ago = 0 is the current block. Record layout
   (spec/vm/memory-layout): slot@0x00 u64, block_time_ns@0x08 u64,
   block_price@0x10 u64, state_root@0x18 [32], block_hash@0x38 [32],
   block_producer@0x58 [32] — 0x78 bytes used per record. */

#define EX10_ERR_BAD_SIZE (0xA000UL)
#define EX10_ERR_BAD_TYPE (0xA001UL)

#define EX10_BASE    (0x0004000000UL)
#define EX10_SPACING (0x1000UL)
#define EX10_RECORD  (0x78UL)

static uint ex10_rd32(uchar const *p) {
    return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) |
           ((uint)p[3] << 24);
}
static ulong ex10_rd64(uchar const *p) {
    return (ulong)ex10_rd32(p) | ((ulong)ex10_rd32(p + 4) << 32);
}

TSDK_ENTRYPOINT_FN void start(uchar const *d, ulong sz) {
    if (sz != 16UL) {
        tsdk_revert(EX10_ERR_BAD_SIZE);
    }
    uint type = ex10_rd32(d);
    uint n = ex10_rd32(d + 4);
    ulong salt = ex10_rd64(d + 8);
    ulong acc = 0;

    switch (type) {
        case 0U: /* read_current: emit the whole 0x78-byte record */
            tsys_emit_event((void const *)EX10_BASE, EX10_RECORD);
            tsdk_return(TSDK_SUCCESS);
            break;
        case 1U: /* read_ago_n: emit the record N blocks back */
            tsys_emit_event(
                (void const *)(EX10_BASE + (ulong)n * EX10_SPACING),
                EX10_RECORD);
            tsdk_return(TSDK_SUCCESS);
            break;
        case 2U: { /* read_many_n: load the slot u64 of N distinct blocks */
            for (uint i = 0; i < n; i++) {
                acc ^= *(volatile ulong const *)
                    (EX10_BASE + (ulong)i * EX10_SPACING);
            }
            __asm__ volatile("" :: "r"(acc));
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 3U: { /* read_beyond: one load at N blocks back, no emit */
            acc = *(volatile ulong const *)
                (EX10_BASE + (ulong)n * EX10_SPACING);
            __asm__ volatile("" :: "r"(acc));
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 4U: { /* commit_reveal draw: past block hash XOR caller salt.
                      NOT safe randomness on its own — the producer of the
                      drawn block can influence its hash. See README. */
            ulong v = *(volatile ulong const *)
                (EX10_BASE + (ulong)n * EX10_SPACING + 0x38UL) ^ salt;
            tsys_emit_event(&v, sizeof(v));
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        default:
            break;
    }
    tsdk_revert(EX10_ERR_BAD_TYPE);
}

#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 09: what accounts and transaction size actually cost.
   One binary; instruction data via entrypoint registers:
   <type u32><n u32><payload...>. Account indices: 0 fee payer, 1 program,
   2.. = declared accounts in transaction order. */

#define EX09_ERR_BAD_SIZE  (0x9000UL)
#define EX09_ERR_BAD_TYPE  (0x9001UL)
#define EX09_ERR_DATA_PTR  (0x9002UL)
#define EX09_ERR_WRITABLE  (0x9003UL)
#define EX09_ERR_CREATE    (0x9004UL)
#define EX09_ERR_RESIZE    (0x9005UL)
#define EX09_ERR_TRANSFER  (0x9006UL)

static uint ex09_rd32(uchar const *p) {
    return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) |
           ((uint)p[3] << 24);
}

TSDK_ENTRYPOINT_FN void start(uchar const *d, ulong sz) {
    if (sz < 8UL) {
        tsdk_revert(EX09_ERR_BAD_SIZE);
    }
    uint type = ex09_rd32(d);
    uint n = ex09_rd32(d + 4);
    ulong acc = 0;

    switch (type) {
        case 0U: /* touch_none */
            tsdk_return(TSDK_SUCCESS);
            break;
        case 1U: { /* read_all: 8 bytes from each of n accounts */
            for (uint i = 0; i < n; i++) {
                volatile ulong const *p = (volatile ulong const *)
                    tsdk_get_account_data_ptr((ushort)(2U + i));
                if (p == NULL) {
                    tsdk_revert(EX09_ERR_DATA_PTR);
                }
                acc += *p;
            }
            __asm__ volatile("" :: "r"(acc));
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 2U: { /* write_all: 1 byte to each of n accounts */
            for (uint i = 0; i < n; i++) {
                ushort idx = (ushort)(2U + i);
                volatile uchar *p =
                    (volatile uchar *)tsdk_get_account_data_ptr(idx);
                if (p == NULL) {
                    tsdk_revert(EX09_ERR_DATA_PTR);
                }
                if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_WRITABLE);
                }
                p[0] = (uchar)(p[0] + 1U);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 3U: { /* echo_n: loop-read n payload bytes */
            if (sz != 8UL + n) {
                tsdk_revert(EX09_ERR_BAD_SIZE);
            }
            for (uint i = 0; i < n; i++) {
                acc += ((volatile uchar const *)d)[8U + i];
            }
            __asm__ volatile("" :: "r"(acc));
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 4U: { /* carry_n: payload rides along, never read */
            if (sz != 8UL + n) {
                tsdk_revert(EX09_ERR_BAD_SIZE);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 5U: { /* create_many: n x [seed32 | psz u32 | proof] */
            uchar const *p = d + 8;
            ulong rem = sz - 8UL;
            for (uint i = 0; i < n; i++) {
                if (rem < 36UL) {
                    tsdk_revert(EX09_ERR_BAD_SIZE);
                }
                uint psz = ex09_rd32(p + 32);
                if (rem < 36UL + psz) {
                    tsdk_revert(EX09_ERR_BAD_SIZE);
                }
                ushort idx = (ushort)(2U + i);
                if (tsys_account_create(idx, p, psz ? p + 36 : NULL, psz)
                        != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_CREATE + ((ulong)i << 16));
                }
                if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_WRITABLE);
                }
                if (tsys_account_resize(idx, 8UL) != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_RESIZE);
                }
                p += 36UL + psz;
                rem -= 36UL + psz;
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 7U: { /* fleet_create_n: <u16 seed_index per slot> x n —
                      creates n EPHEMERAL accounts (no proofs), each with an
                      8-byte data region. Seed = 32 zero bytes with the LE
                      u32 seed index at offset 0; the per-slot index list is
                      needed because account lists are sorted, seeds are not. */
            if (sz != 8UL + 2UL * n) {
                tsdk_revert(EX09_ERR_BAD_SIZE);
            }
            for (uint i = 0; i < n; i++) {
                uchar seed[32];
                for (uint k = 0; k < 32U; k++) {
                    seed[k] = 0;
                }
                uint sidx = (uint)d[8 + 2 * i] | ((uint)d[9 + 2 * i] << 8);
                seed[0] = (uchar)sidx;
                seed[1] = (uchar)(sidx >> 8);
                ushort idx = (ushort)(2U + i);
                ulong r = tsys_account_create_ephemeral(idx, seed);
                if (r != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_CREATE + ((ulong)i << 16));
                }
                if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_WRITABLE);
                }
                if (tsys_account_resize(idx, 8UL) != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_RESIZE);
                }
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 6U: { /* pay_all: treasury at idx 2 pays 1 token to idx 3..3+n-1 */
            for (uint i = 0; i < n; i++) {
                if (tsys_account_transfer(2UL, 3UL + i, 1UL)
                        != TSDK_SUCCESS) {
                    tsdk_revert(EX09_ERR_TRANSFER + ((ulong)i << 16));
                }
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        default:
            break;
    }
    tsdk_revert(EX09_ERR_BAD_TYPE);
}

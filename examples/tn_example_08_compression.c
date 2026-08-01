#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

/* Example 08: account compression round trip. One binary; instruction data
   parsed byte-wise (unaligned-safe): <type u32><idx u16><payload...>.
   Types: 0 compress(proof), 1 decompress(meta62+data+proof), 2 modify,
   3 recompress(proof), 4 create(seed32+proof), 5 resize(u32). */

static uint ex08_rd32(uchar const *p) {
    return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) |
           ((uint)p[3] << 24);
}
static ushort ex08_rd16(uchar const *p) {
    return (ushort)((uint)p[0] | ((uint)p[1] << 8));
}

TSDK_ENTRYPOINT_FN void start(uchar const *d, ulong sz) {
    if (sz < 6UL) {
        tsdk_revert(0x8000UL);
    }
    uint type = ex08_rd32(d);
    ushort idx = ex08_rd16(d + 4);
    uchar const *p = d + 6;
    ulong rem = sz - 6UL;

    switch (type) {
        case 0U:
        case 3U: { /* compress / recompress: <proof_sz u32><proof> */
            if (rem < 4UL) {
                tsdk_revert(0x8001UL);
            }
            uint psz = ex08_rd32(p);
            if (rem != 4UL + psz) {
                tsdk_revert(0x8001UL);
            }
            ulong r = tsys_account_compress(idx, p + 4, psz);
            if (r != TSDK_SUCCESS) {
                tsdk_revert(0x8100UL + r);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 1U: { /* decompress: <meta 62><data_sz u32><data><proof_sz u32><proof> */
            if (rem < 66UL) {
                tsdk_revert(0x8002UL);
            }
            uint dsz = ex08_rd32(p + 62);
            if (rem < 66UL + dsz + 4UL) {
                tsdk_revert(0x8002UL);
            }
            uint psz = ex08_rd32(p + 66 + dsz);
            if (rem != 70UL + dsz + psz) {
                tsdk_revert(0x8002UL);
            }
            ulong r = tsys_account_decompress(idx, p, p + 66,
                                              p + 70 + dsz, psz);
            if (r != TSDK_SUCCESS) {
                tsdk_revert(0x8200UL + r);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 2U: { /* modify: bump byte 0 */
            volatile uchar *a =
                (volatile uchar *)tsdk_get_account_data_ptr(idx);
            if (a == NULL) {
                tsdk_revert(0x8003UL);
            }
            if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
                tsdk_revert(0x8004UL);
            }
            a[0] = (uchar)(a[0] + 1U);
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 4U: { /* create: <seed 32><proof_sz u32><proof> */
            if (rem < 36UL) {
                tsdk_revert(0x8005UL);
            }
            uint psz = ex08_rd32(p + 32);
            if (rem != 36UL + psz) {
                tsdk_revert(0x8005UL);
            }
            if (tsys_account_create(idx, p, psz ? p + 36 : NULL, psz)
                    != TSDK_SUCCESS) {
                tsdk_revert(0x8006UL);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        case 5U: { /* resize: <new_size u32> */
            if (rem != 4UL) {
                tsdk_revert(0x8007UL);
            }
            if (tsys_set_account_data_writable(idx) != TSDK_SUCCESS) {
                tsdk_revert(0x8008UL);
            }
            if (tsys_account_resize(idx, ex08_rd32(p)) != TSDK_SUCCESS) {
                tsdk_revert(0x8009UL);
            }
            tsdk_return(TSDK_SUCCESS);
            break;
        }
        default:
            break;
    }
    tsdk_revert(0x800AUL);
}

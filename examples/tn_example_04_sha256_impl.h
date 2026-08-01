#ifndef TN_EXAMPLE_04_SHA256_IMPL_H
#define TN_EXAMPLE_04_SHA256_IMPL_H

/* Example 04: SHA-256 core shared by both arms. The including .c file must
   define SHA256_SSIG0/SSIG1/BSIG0/BSIG1 (the four sigma functions) before
   including this header; everything else — message schedule, rounds, padding,
   entrypoint — is byte-for-byte identical between arms so CU deltas isolate
   the sigma implementations. */

#include <stddef.h>
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

static const uint32_t ex04_k256[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static void ex04_compress(uint32_t st[8], uchar const *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SHA256_SSIG1(w[i - 2]) + w[i - 7] +
               SHA256_SSIG0(w[i - 15]) + w[i - 16];
    }
    uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
    uint32_t e = st[4], f = st[5], g = st[6], h = st[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + SHA256_BSIG1(e) + ((e & f) ^ (~e & g)) +
                      ex04_k256[i] + w[i];
        uint32_t t2 = SHA256_BSIG0(a) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
    st[4] += e; st[5] += f; st[6] += g; st[7] += h;
}

static void ex04_sha256(uchar const *msg, ulong len, uchar out[32]) {
    uint32_t st[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    ulong off = 0;
    while (len - off >= 64) {
        ex04_compress(st, msg + off);
        off += 64;
    }
    uchar tail[128];
    ulong rem = len - off;
    for (ulong i = 0; i < rem; i++) {
        tail[i] = msg[off + i];
    }
    tail[rem] = 0x80U;
    ulong tlen = (rem + 9 <= 64) ? 64 : 128;
    for (ulong i = rem + 1; i < tlen - 8; i++) {
        tail[i] = 0;
    }
    ulong bits = len * 8;
    for (int i = 0; i < 8; i++) {
        tail[tlen - 8 + (ulong)i] = (uchar)(bits >> (56 - 8 * i));
    }
    ex04_compress(st, tail);
    if (tlen == 128) {
        ex04_compress(st, tail + 64);
    }
    for (int i = 0; i < 8; i++) {
        out[4 * i]     = (uchar)(st[i] >> 24);
        out[4 * i + 1] = (uchar)(st[i] >> 16);
        out[4 * i + 2] = (uchar)(st[i] >> 8);
        out[4 * i + 3] = (uchar)st[i];
    }
}

/* Whole instruction-data payload is the message. The digest is emitted as an
   event so the hash cannot be eliminated as dead code; the emit syscall and
   32 event bytes are constant across arms and sizes, so they cancel in deltas. */
TSDK_ENTRYPOINT_FN void start(void) {
    tsdk_txn_t const *txn = tsdk_get_txn();
    uchar const *instruction_data = tsdk_txn_get_instr_data(txn);
    ulong instruction_data_sz = tsdk_txn_get_instr_data_sz(txn);

    uchar digest[32];
    ex04_sha256(instruction_data, instruction_data_sz, digest);
    tsys_emit_event(digest, 32);
    tsdk_return(TSDK_SUCCESS);
}

#endif

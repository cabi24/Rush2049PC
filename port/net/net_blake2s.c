/* port/net/net_blake2s.c: BLAKE2s as RFC 7693 specifies it (sections 2.1-3.3, 32-bit words, 10 rounds,
 * rotations 16, 12, 8, 7), written from the RFC. Checked against its test vector (appendix B) and Python's
 * hashlib.blake2s in net_test. */
#include "net_blake2s.h"

#include <string.h>

static const uint32_t IV[8] = {0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
                               0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u};

static const uint8_t SIGMA[10][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4}, {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13}, {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11}, {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5}, {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
};

static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

#define G(a, b, c, d, x, y)                                                                                           \
    do {                                                                                                              \
        v[a] = v[a] + v[b] + (x);                                                                                     \
        v[d] = rotr(v[d] ^ v[a], 16);                                                                                 \
        v[c] = v[c] + v[d];                                                                                           \
        v[b] = rotr(v[b] ^ v[c], 12);                                                                                 \
        v[a] = v[a] + v[b] + (y);                                                                                     \
        v[d] = rotr(v[d] ^ v[a], 8);                                                                                  \
        v[c] = v[c] + v[d];                                                                                           \
        v[b] = rotr(v[b] ^ v[c], 7);                                                                                  \
    } while (0)

/* F: compress one 64-byte block; last marks the final block (f0 = all ones). */
static void compress(NetBlake2s *c, const uint8_t *block, int last)
{
    uint32_t v[16], m[16];
    for (int i = 0; i < 16; i++)
        m[i] = load32(block + 4 * i);
    for (int i = 0; i < 8; i++) {
        v[i] = c->h[i];
        v[i + 8] = IV[i];
    }
    v[12] ^= c->t[0];
    v[13] ^= c->t[1];
    if (last)
        v[14] = ~v[14];
    for (int r = 0; r < 10; r++) {
        const uint8_t *s = SIGMA[r];
        G(0, 4, 8, 12, m[s[0]], m[s[1]]);
        G(1, 5, 9, 13, m[s[2]], m[s[3]]);
        G(2, 6, 10, 14, m[s[4]], m[s[5]]);
        G(3, 7, 11, 15, m[s[6]], m[s[7]]);
        G(0, 5, 10, 15, m[s[8]], m[s[9]]);
        G(1, 6, 11, 12, m[s[10]], m[s[11]]);
        G(2, 7, 8, 13, m[s[12]], m[s[13]]);
        G(3, 4, 9, 14, m[s[14]], m[s[15]]);
    }
    for (int i = 0; i < 8; i++)
        c->h[i] ^= v[i] ^ v[i + 8];
}

static void add_count(NetBlake2s *c, uint32_t n)
{
    c->t[0] += n;
    if (c->t[0] < n)
        c->t[1]++;
}

int net_blake2s_init(NetBlake2s *c, size_t outlen, const void *key, size_t keylen)
{
    if (outlen == 0 || outlen > 32 || keylen > 32 || (keylen && !key))
        return -1;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < 8; i++)
        c->h[i] = IV[i];
    c->h[0] ^= 0x01010000u ^ ((uint32_t)keylen << 8) ^ (uint32_t)outlen;   /* the parameter block */
    c->outlen = outlen;
    if (keylen) {   /* the key, zero-padded to a block, is the first block */
        memcpy(c->buf, key, keylen);
        c->fill = 64;
    }
    return 0;
}

void net_blake2s_update(NetBlake2s *c, const void *data, size_t n)
{
    const uint8_t *p = data;
    while (n > 0) {
        if (c->fill == 64) {   /* a full block is compressed only once more data follows (the last is final) */
            add_count(c, 64);
            compress(c, c->buf, 0);
            c->fill = 0;
        }
        size_t take = 64 - c->fill;
        if (take > n)
            take = n;
        memcpy(c->buf + c->fill, p, take);
        c->fill += take;
        p += take;
        n -= take;
    }
}

void net_blake2s_final(NetBlake2s *c, void *out)
{
    uint8_t full[32];
    add_count(c, (uint32_t)c->fill);
    memset(c->buf + c->fill, 0, 64 - c->fill);
    compress(c, c->buf, 1);
    for (int i = 0; i < 8; i++) {
        full[4 * i] = (uint8_t)c->h[i];
        full[4 * i + 1] = (uint8_t)(c->h[i] >> 8);
        full[4 * i + 2] = (uint8_t)(c->h[i] >> 16);
        full[4 * i + 3] = (uint8_t)(c->h[i] >> 24);
    }
    memcpy(out, full, c->outlen);
}

void net_blake2s(void *out, size_t outlen, const void *key, size_t keylen, const void *data, size_t n)
{
    NetBlake2s c;
    net_blake2s_init(&c, outlen, key, keylen);
    net_blake2s_update(&c, data, n);
    net_blake2s_final(&c, out);
}

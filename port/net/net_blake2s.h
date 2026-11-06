/* port/net/net_blake2s.h: BLAKE2s (RFC 7693), unkeyed and keyed, written from the RFC's description.
 * The network library hashes what the two PCs agree on (settings, files) with it, and, with a join code,
 * authenticates every packet with its keyed form (docs/port/NETWORK.md). */
#ifndef NET_BLAKE2S_H
#define NET_BLAKE2S_H

#include <stddef.h>
#include <stdint.h>

typedef struct NetBlake2s {
    uint32_t h[8], t[2];
    uint8_t buf[64];
    size_t fill, outlen;
} NetBlake2s;

/* outlen 1..32; key NULL or keylen 0 for the unkeyed hash, else keylen 1..32. 0 on success. */
int net_blake2s_init(NetBlake2s *c, size_t outlen, const void *key, size_t keylen);
void net_blake2s_update(NetBlake2s *c, const void *data, size_t n);
void net_blake2s_final(NetBlake2s *c, void *out);
/* One call: out[outlen] = BLAKE2s(key, data). */
void net_blake2s(void *out, size_t outlen, const void *key, size_t keylen, const void *data, size_t n);

#endif

/* port/net/net_proto.c: reading and writing port/net's packets (net_proto.h, docs/port/NETWORK.md 3). */
#include "net_proto.h"

#include <string.h>

#include "net_blake2s.h"

uint16_t net_get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t net_get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
uint64_t net_get64(const uint8_t *p) { return (uint64_t)net_get32(p) | (uint64_t)net_get32(p + 4) << 32; }

int net_popcount4(uint8_t ports)
{
    int n = 0;
    for (int i = 0; i < 4; i++)
        n += ports >> i & 1;
    return n;
}

const char *net_msg_name(int type)
{
    static const char *names[NET_MSG_COUNT] = {"?",     "HELLO",  "HELLO_ACK", "SETTINGS", "FILE", "FILE_ACK", "READY",
                                               "START", "INPUT",  "HASH",      "PING",     "PONG", "BYE"};
    return type > 0 && type < NET_MSG_COUNT ? names[type] : "?";
}

/* ---------------------------------------------------------------- reading */

typedef struct Rd {
    const uint8_t *p;
    int len, off, bad;
} Rd;

static const uint8_t *rd_take(Rd *r, int n)
{
    if (n < 0 || r->bad || n > r->len - r->off) {
        r->bad = 1;
        return NULL;
    }
    const uint8_t *q = r->p + r->off;
    r->off += n;
    return q;
}

static uint8_t rd_u8(Rd *r)
{
    const uint8_t *q = rd_take(r, 1);
    return q ? q[0] : 0;
}

static uint16_t rd_u16(Rd *r)
{
    const uint8_t *q = rd_take(r, 2);
    return q ? net_get16(q) : 0;
}

static uint32_t rd_u32(Rd *r)
{
    const uint8_t *q = rd_take(r, 4);
    return q ? net_get32(q) : 0;
}

static uint64_t rd_u64(Rd *r)
{
    const uint8_t *q = rd_take(r, 8);
    return q ? net_get64(q) : 0;
}

static void rd_bytes(Rd *r, void *out, int n)
{
    const uint8_t *q = rd_take(r, n);
    if (q)
        memcpy(out, q, (size_t)n);
    else
        memset(out, 0, (size_t)n);
}

/* A fixed-size text field: NUL-terminated within it, printable ASCII only. */
static int rd_text(Rd *r, char *out, int n)
{
    rd_bytes(r, out, n);
    if (r->bad)
        return -1;
    if (!memchr(out, 0, (size_t)n))
        return -1;
    for (int i = 0; i < n && out[i]; i++)
        if ((unsigned char)out[i] < 0x20 || (unsigned char)out[i] > 0x7E)
            return -1;
    return 0;
}

int net_proto_parse_header(const uint8_t *buf, int len, NetHeader *h)
{
    if (len < NET_HEADER_SIZE)
        return NET_PARSE_SHORT;
    if (len > NET_MAX_PACKET)
        return NET_PARSE_LONG;
    if (buf[0] != NET_MAGIC0 || buf[1] != NET_MAGIC1 || buf[2] != NET_MAGIC2 || buf[3] != NET_MAGIC3)
        return NET_PARSE_MAGIC;
    h->version = buf[4];
    h->type = buf[5];
    h->flags = buf[6];
    h->sender = buf[7];
    h->session = net_get64(buf + 8);
    h->seq = net_get32(buf + 16);
    h->t_send = net_get64(buf + 20);
    h->echo = net_get64(buf + 28);
    h->hold_us = net_get32(buf + 36);
    if (h->version != NET_PROTO_VERSION)
        return NET_PARSE_VERSION;
    if (h->type == 0 || h->type >= NET_MSG_COUNT)
        return NET_PARSE_TYPE;
    if ((h->flags & ~NET_FLAG_TAG) || h->sender > 1)
        return NET_PARSE_FIELD;
    return NET_PARSE_OK;
}

static int parse_blob_info(Rd *r, NetBlobInfo *b)
{
    b->size = rd_u32(r);
    rd_bytes(r, b->hash, NET_HASH_LEN);
    return r->bad ? -1 : 0;
}

int net_proto_parse(const uint8_t *buf, int len, NetMsg *m)
{
    memset(m, 0, sizeof *m);
    int e = net_proto_parse_header(buf, len, &m->h);
    if (e)
        return e;
    m->has_tag = (m->h.flags & NET_FLAG_TAG) != 0;
    int end = len - (m->has_tag ? NET_TAG_SIZE : 0);
    if (end < NET_HEADER_SIZE)
        return NET_PARSE_SHORT;
    Rd r = {buf, end, NET_HEADER_SIZE, 0};
    switch (m->h.type) {
    case NET_MSG_HELLO:
    case NET_MSG_HELLO_ACK: {
        NetHello *h = &m->u.hello;
        h->nonce = rd_u64(&r);
        h->nonce_echo = rd_u64(&r);
        h->ports = rd_u8(&r);
        h->nfiles = rd_u8(&r);
        if (rd_u16(&r) != 0)
            return NET_PARSE_FIELD;
        parse_blob_info(&r, &h->settings);
        if (rd_text(&r, h->build, NET_BUILD_LEN) != 0)
            return r.bad ? NET_PARSE_SHORT : NET_PARSE_FIELD;
        if (h->nfiles > NET_MAX_FILES || h->ports == 0 || (h->ports & 0xF0))
            return NET_PARSE_FIELD;
        for (int i = 0; i < h->nfiles; i++)
            parse_blob_info(&r, &h->files[i]);
        break;
    }
    case NET_MSG_SETTINGS:
    case NET_MSG_FILE: {
        NetChunk *c = &m->u.chunk;
        c->index = rd_u8(&r);
        if (rd_u8(&r) != 0)
            return NET_PARSE_FIELD;
        c->chunk = rd_u16(&r);
        c->total = rd_u32(&r);
        c->len = rd_u16(&r);
        if (rd_u16(&r) != 0)
            return NET_PARSE_FIELD;
        if (c->len == 0 || c->len > NET_CHUNK)
            return r.bad ? NET_PARSE_SHORT : NET_PARSE_FIELD;
        c->data = rd_take(&r, c->len);
        break;
    }
    case NET_MSG_FILE_ACK: {
        NetChunkAck *a = &m->u.chunk_ack;
        a->kind = rd_u8(&r);
        a->index = rd_u8(&r);
        a->nchunks = rd_u16(&r);
        if (a->kind > NET_BLOB_FILE || a->nchunks == 0)
            return r.bad ? NET_PARSE_SHORT : NET_PARSE_FIELD;
        a->bitmap = rd_take(&r, (a->nchunks + 7) / 8);
        break;
    }
    case NET_MSG_READY:
        rd_bytes(&r, m->u.ready_hash, NET_HASH_LEN);
        break;
    case NET_MSG_START:
        rd_bytes(&r, m->u.start.hash, NET_HASH_LEN);
        m->u.start.start_host_ns = rd_u64(&r);
        m->u.start.clock = (int64_t)rd_u64(&r);
        m->u.start.clock_valid = rd_u8(&r);
        if (m->u.start.clock_valid > 1 || rd_u8(&r) || rd_u16(&r))
            return r.bad ? NET_PARSE_SHORT : NET_PARSE_FIELD;
        break;
    case NET_MSG_INPUT:
    case NET_MSG_HASH: {
        NetInput *in = &m->u.input;
        in->cur = rd_u32(&r);
        in->cur_start = rd_u64(&r);
        in->ack = rd_u32(&r);
        in->hash_ack = rd_u32(&r);
        in->whash_ack = rd_u32(&r);
        in->first = rd_u32(&r);
        in->count = rd_u8(&r);
        in->ports = rd_u8(&r);
        in->delay = rd_u8(&r);
        in->flags = rd_u8(&r);
        in->feedback_us = rd_u32(&r);
        in->clock = (int64_t)rd_u64(&r);
        in->hash_first = rd_u32(&r);
        in->hash_count = rd_u16(&r);
        if (rd_u16(&r) != 0)
            return NET_PARSE_FIELD;
        if (r.bad)
            return NET_PARSE_SHORT;
        if (in->count > NET_WINDOW || in->hash_count > NET_MAX_HASHES || in->ports == 0 || (in->ports & 0xF0) ||
            (in->flags & ~(NET_INPUT_WHASH | NET_INPUT_FEEDBACK | NET_INPUT_CLOCK)))
            return NET_PARSE_FIELD;
        if (m->h.type == NET_MSG_HASH && in->count)
            return NET_PARSE_FIELD;
        if ((uint64_t)in->first + in->count > NET_RETRACE_NONE ||
            (uint64_t)in->hash_first + in->hash_count > NET_RETRACE_NONE)
            return NET_PARSE_FIELD;
        in->pads = rd_take(&r, in->count * net_popcount4(in->ports) * 4);
        in->ages = rd_take(&r, in->count * 2);
        in->hashes = rd_take(&r, in->hash_count * 4);
        if (in->flags & NET_INPUT_WHASH) {
            in->whash_retrace = rd_u32(&r);
            in->whash = rd_u64(&r);
        }
        break;
    }
    case NET_MSG_PING:
    case NET_MSG_PONG:
        m->u.ping_id = rd_u32(&r);
        break;
    case NET_MSG_BYE: {
        NetBye *b = &m->u.bye;
        b->reason = rd_u8(&r);
        rd_take(&r, 3);
        b->detail = rd_u32(&r);
        if (rd_text(&r, b->text, NET_BYE_TEXT) != 0)
            return r.bad ? NET_PARSE_SHORT : NET_PARSE_FIELD;
        break;
    }
    default:
        return NET_PARSE_TYPE;
    }
    if (r.bad)
        return NET_PARSE_SHORT;
    if (r.off != end)
        return NET_PARSE_LONG;
    return NET_PARSE_OK;
}

/* ---------------------------------------------------------------- writing */

void net_w_init(NetWriter *w, uint8_t *buf, int cap)
{
    w->p = buf;
    w->len = 0;
    w->cap = cap;
    w->over = 0;
}

void net_w_bytes(NetWriter *w, const void *b, int n)
{
    if (w->over || n > w->cap - w->len) {
        w->over = 1;
        return;
    }
    if (b)
        memcpy(w->p + w->len, b, (size_t)n);
    else
        memset(w->p + w->len, 0, (size_t)n);
    w->len += n;
}

void net_w_u8(NetWriter *w, uint8_t v) { net_w_bytes(w, &v, 1); }

void net_w_u16(NetWriter *w, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    net_w_bytes(w, b, 2);
}

void net_w_u32(NetWriter *w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    net_w_bytes(w, b, 4);
}

void net_w_u64(NetWriter *w, uint64_t v)
{
    net_w_u32(w, (uint32_t)v);
    net_w_u32(w, (uint32_t)(v >> 32));
}

void net_w_header(NetWriter *w, const NetHeader *h)
{
    uint8_t magic[4] = {NET_MAGIC0, NET_MAGIC1, NET_MAGIC2, NET_MAGIC3};
    net_w_bytes(w, magic, 4);
    net_w_u8(w, h->version);
    net_w_u8(w, h->type);
    net_w_u8(w, h->flags);
    net_w_u8(w, h->sender);
    net_w_u64(w, h->session);
    net_w_u32(w, h->seq);
    net_w_u64(w, h->t_send);
    net_w_u64(w, h->echo);
    net_w_u32(w, h->hold_us);
}

static void w_text(NetWriter *w, const char *s, int n)
{
    char buf[256] = {0};
    size_t l = strlen(s);
    if (l > (size_t)n - 1)
        l = (size_t)n - 1;
    memcpy(buf, s, l);
    net_w_bytes(w, buf, n);
}

void net_w_hello(NetWriter *w, const NetHello *m)
{
    net_w_u64(w, m->nonce);
    net_w_u64(w, m->nonce_echo);
    net_w_u8(w, m->ports);
    net_w_u8(w, m->nfiles);
    net_w_u16(w, 0);
    net_w_u32(w, m->settings.size);
    net_w_bytes(w, m->settings.hash, NET_HASH_LEN);
    w_text(w, m->build, NET_BUILD_LEN);
    for (int i = 0; i < m->nfiles && i < NET_MAX_FILES; i++) {
        net_w_u32(w, m->files[i].size);
        net_w_bytes(w, m->files[i].hash, NET_HASH_LEN);
    }
}

void net_w_chunk(NetWriter *w, const NetChunk *c)
{
    net_w_u8(w, c->index);
    net_w_u8(w, 0);
    net_w_u16(w, c->chunk);
    net_w_u32(w, c->total);
    net_w_u16(w, c->len);
    net_w_u16(w, 0);
    net_w_bytes(w, c->data, c->len);
}

void net_w_bye(NetWriter *w, const NetBye *b)
{
    net_w_u8(w, b->reason);
    net_w_bytes(w, NULL, 3);
    net_w_u32(w, b->detail);
    w_text(w, b->text, NET_BYE_TEXT);
}

/* ---------------------------------------------------------------- the join code */

void net_proto_key(const char *code, uint8_t key[32])
{
    static const char label[] = "rush2049pc join code v1";
    NetBlake2s c;
    net_blake2s_init(&c, 32, NULL, 0);
    net_blake2s_update(&c, label, sizeof label);   /* with its NUL: the label ends there */
    net_blake2s_update(&c, code, strlen(code));
    net_blake2s_final(&c, key);
}

void net_proto_tag(const uint8_t key[32], const uint8_t *pkt, int len_without_tag, uint8_t tag[NET_TAG_SIZE])
{
    net_blake2s(tag, NET_TAG_SIZE, key, 32, pkt, (size_t)len_without_tag);
}

int net_proto_tag_ok(const uint8_t key[32], const uint8_t *pkt, int len)
{
    uint8_t t[NET_TAG_SIZE], d = 0;
    if (len < NET_HEADER_SIZE + NET_TAG_SIZE)
        return 0;
    net_proto_tag(key, pkt, len - NET_TAG_SIZE, t);
    for (int i = 0; i < NET_TAG_SIZE; i++)
        d |= (uint8_t)(t[i] ^ pkt[len - NET_TAG_SIZE + i]);
    return d == 0;
}

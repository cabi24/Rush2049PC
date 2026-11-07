/* port/net/net_proto.h: the wire format of port/net (docs/port/NETWORK.md 3). Every field little-endian, every
 * layout fixed; a packet is a 40-byte header, a payload, and with a join code a 16-byte tag. The parsers check
 * every length and every count against the bytes there are, and nothing else: what values mean in a session
 * (windows, states, the peer) is net.c's to check. */
#ifndef NET_PROTO_H
#define NET_PROTO_H

#include <stddef.h>
#include <stdint.h>

#define NET_MAGIC0 'R'
#define NET_MAGIC1 '2'
#define NET_MAGIC2 'N'
#define NET_MAGIC3 'P'
#define NET_PROTO_VERSION 2   /* 2: rejoins (HELLO's rejoin block, PROGRESS), chunks numbered to 64 MiB */
#define NET_MAX_PACKET 1200   /* Tailscale's MTU is 1280 */
#define NET_HEADER_SIZE 40
#define NET_TAG_SIZE 16
#define NET_CHUNK 1024
#define NET_WINDOW 120         /* inputs in one packet, at most */
#define NET_MAX_HASHES 256     /* hashes in one packet, at most */
#define NET_MAX_FILES 8
#define NET_BUILD_LEN 48
#define NET_BYE_TEXT 64
#define NET_HASH_LEN 32
#define NET_RETRACE_NONE 0xFFFFFFFFu
#define NET_ACK_BYTES 1024     /* a FILE_ACK's bitmap, at most: 8192 chunks past its base */
#define NET_MAX_CHUNKS 65536   /* a blob's chunks, at most (64 MiB) */

#define NET_HELLO_REJOIN 0x01  /* HELLO's rejoin flags: this side rebuilds the session (it lost its game) */
#define NET_HELLO_KEEP 0x02    /* ... this side keeps the session and offers its log */

#define NET_FLAG_TAG 0x01      /* header flags */

enum NetMsgType {
    NET_MSG_HELLO = 1,
    NET_MSG_HELLO_ACK = 2,
    NET_MSG_SETTINGS = 3,
    NET_MSG_FILE = 4,
    NET_MSG_FILE_ACK = 5,
    NET_MSG_READY = 6,
    NET_MSG_START = 7,
    NET_MSG_INPUT = 8,
    NET_MSG_HASH = 9,
    NET_MSG_PING = 10,
    NET_MSG_PONG = 11,
    NET_MSG_BYE = 12,
    NET_MSG_PROGRESS = 13,   /* a rejoining side's replay: where it is */
    NET_MSG_COUNT
};

typedef struct NetHeader {
    uint8_t version, type, flags, sender;   /* sender: 0 host, 1 guest */
    uint64_t session;
    uint32_t seq;
    uint64_t t_send;   /* the sender's clock, ns */
    uint64_t echo;     /* t_send of the last packet the sender received from the receiver; 0 none */
    uint32_t hold_us;  /* from that packet's arrival to this send */
} NetHeader;

typedef struct NetBlobInfo {
    uint32_t size;
    uint8_t hash[NET_HASH_LEN];   /* BLAKE2s-256 of the bytes; zero when size is 0 */
} NetBlobInfo;

typedef struct NetHello {   /* HELLO and HELLO_ACK */
    uint64_t nonce, nonce_echo;
    uint8_t ports, nfiles;
    uint8_t rejoin;              /* NET_HELLO_REJOIN, NET_HELLO_KEEP, or 0 (a session of its own) */
    NetBlobInfo settings;
    char build[NET_BUILD_LEN];   /* NUL-terminated */
    /* the rejoin block (zero without one): the session's id; with KEEP, the retrace it goes on from, the desync it
     * recovers from (NET_RETRACE_NONE: none), and how the files are the host's, the guest's, then the log */
    uint64_t session;
    uint32_t live, recover_at;
    uint8_t host_files, guest_files;
    NetBlobInfo files[NET_MAX_FILES];
} NetHello;

typedef struct NetChunk {   /* SETTINGS and FILE */
    uint8_t index;
    uint32_t chunk;
    uint32_t total;
    uint16_t len;
    const uint8_t *data;
} NetChunk;

enum { NET_BLOB_SETTINGS = 0, NET_BLOB_FILE = 1 };

typedef struct NetChunkAck {
    uint8_t kind, index;
    uint16_t nbytes;          /* the bitmap's bytes, at most NET_ACK_BYTES */
    uint32_t nchunks;         /* the blob's chunks */
    uint32_t base;            /* every chunk before it has arrived */
    const uint8_t *bitmap;    /* bit i: chunk base + i has arrived */
} NetChunkAck;

typedef struct NetProgress {   /* PROGRESS: a rejoining side's replay */
    uint32_t at, live;
} NetProgress;

typedef struct NetStartMsg {
    uint8_t hash[NET_HASH_LEN];
    uint64_t start_host_ns;
    int64_t clock;      /* the host's estimate of (guest clock - host clock), ns */
    uint8_t clock_valid;
} NetStartMsg;

#define NET_INPUT_WHASH 0x01
#define NET_INPUT_FEEDBACK 0x02
#define NET_INPUT_CLOCK 0x04

typedef struct NetInput {   /* INPUT and HASH */
    uint32_t cur;        /* the sender's current retrace; NET_RETRACE_NONE before retrace 0 */
    uint64_t cur_start;  /* when it began, sender's clock */
    uint32_t ack;        /* the receiver's inputs the sender has, contiguous from 0: the next it expects */
    uint32_t hash_ack;   /* the same for hashes */
    uint32_t whash_ack;  /* the latest window hash received, its retrace + 1; 0 none */
    uint32_t first;      /* the retrace of the first input here */
    uint8_t count, ports, delay, flags;
    uint32_t feedback_us;   /* the slowest of the receiver's inputs to arrive lately (NET_INPUT_FEEDBACK) */
    int64_t clock;          /* the sender's estimate of (receiver clock - sender clock), ns (NET_INPUT_CLOCK):
                               the host's is the one both sides use */
    uint32_t hash_first;
    uint16_t hash_count;
    const uint8_t *pads;    /* count x popcount(ports) x 4 bytes */
    const uint8_t *ages;    /* count x u16: 100 us units from each input's assignment to this send */
    const uint8_t *hashes;  /* hash_count x u32 */
    uint32_t whash_retrace;
    uint64_t whash;
} NetInput;

typedef struct NetBye {
    uint8_t reason;
    uint32_t detail;
    char text[NET_BYE_TEXT];   /* NUL-terminated */
} NetBye;

typedef struct NetMsg {
    NetHeader h;
    int has_tag;
    union {
        NetHello hello;
        NetChunk chunk;
        NetChunkAck chunk_ack;
        uint8_t ready_hash[NET_HASH_LEN];
        NetStartMsg start;
        NetInput input;
        uint32_t ping_id;
        NetBye bye;
        NetProgress progress;
    } u;
} NetMsg;

enum NetParseError {
    NET_PARSE_OK = 0,
    NET_PARSE_SHORT = -1,      /* shorter than a header, or than its payload says */
    NET_PARSE_LONG = -2,       /* over NET_MAX_PACKET, or bytes left over */
    NET_PARSE_MAGIC = -3,
    NET_PARSE_VERSION = -4,
    NET_PARSE_TYPE = -5,
    NET_PARSE_FIELD = -6,      /* a count or field out of its range */
};

/* Parse a whole datagram. The tag (when the header says so) is not checked here: net_proto_tag_ok. Pointers in
 * the message point into buf. */
int net_proto_parse(const uint8_t *buf, int len, NetMsg *m);
/* Only the header (the version check before the rest). */
int net_proto_parse_header(const uint8_t *buf, int len, NetHeader *h);
int net_popcount4(uint8_t ports);

/* Building packets: a buffer of NET_MAX_PACKET bytes. Each writer returns the length, or 0 when it doesn't fit. */
typedef struct NetWriter {
    uint8_t *p;
    int len, cap, over;
} NetWriter;

void net_w_init(NetWriter *w, uint8_t *buf, int cap);
void net_w_u8(NetWriter *w, uint8_t v);
void net_w_u16(NetWriter *w, uint16_t v);
void net_w_u32(NetWriter *w, uint32_t v);
void net_w_u64(NetWriter *w, uint64_t v);
void net_w_bytes(NetWriter *w, const void *b, int n);
void net_w_header(NetWriter *w, const NetHeader *h);
void net_w_hello(NetWriter *w, const NetHello *m);
void net_w_chunk(NetWriter *w, const NetChunk *c);
void net_w_bye(NetWriter *w, const NetBye *b);

uint16_t net_get16(const uint8_t *p);
uint32_t net_get32(const uint8_t *p);
uint64_t net_get64(const uint8_t *p);

/* The join code's key, and the tag: BLAKE2s-128 keyed with it over the packet before the tag. */
void net_proto_key(const char *code, uint8_t key[32]);
void net_proto_tag(const uint8_t key[32], const uint8_t *pkt, int len_without_tag, uint8_t tag[NET_TAG_SIZE]);
/* 1 when the last NET_TAG_SIZE bytes of pkt are its tag (constant time). */
int net_proto_tag_ok(const uint8_t key[32], const uint8_t *pkt, int len);

const char *net_msg_name(int type);

#endif

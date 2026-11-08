/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudAddListener: allocate and post a four-vector (60-byte records of the pool gListeners
 * that AudInit3D builds; free list gFreeListeners, posted list gActiveListeners).
 *   if the pool does not exist return -1; take the lock (osRecvMesg on audio_lock_queue, blocking);
 *   p = free.head, unlink it (RemoveLLE), p->type (+8) = 0; copy the four Vec3 a..d into
 *   p+12/24/36/48; insert at the head of the posted list (InsertLLE(list, p, list->head));
 *   p->ready (+9) = 1; release the lock (osJamMesg); return p.
 * Shape (reconstruction, sweep sw1/P0_h re-scored):
 *   - allocate, post, lock and unlock are four static helpers inlined by umerge (frame 48 and the
 *     separate homes 28/24/44 are their inlined locals); no retail stubs remain for them.
 *   - SHAPING DEVICE (disclosed): one compiled-out read `if (gFreeListeners.head) {}` right after the
 *     lock (probably a compiled-out empty-list assertion). It splits the block so the four
 *     parameter webs are reloaded and re-spilled around RemoveLLE and the head load / home
 *     store land where retail has them (without it: 10 of 81 words, as1 order of one spill store).
 * Whole-program unit: EQUAL (the whole-program unit --neighbours, 0 locked bodies differ). No own.rodata.
 */
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
#define NULL ((void *)0)
typedef unsigned char u8;
typedef unsigned int u32;
typedef struct Link {struct Link *next,*prev;} Link;
typedef struct List {u8 indirect,doubly,pad[2];u32 count;Link *head,*tail;} List;
typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Packet {Link link;u8 type,ready,opaque[2];Vec3 a,b,c,d;} Packet;
typedef struct OSMesgQueue OSMesgQueue;
extern OSMesgQueue audio_lock_queue;
extern List gFreeListeners,gActiveListeners;
extern void *gListeners;
extern int osRecvMesg(OSMesgQueue *,void **,int);
extern int osSendMesg(OSMesgQueue *,void *,int);
extern void RemoveLLE(List *,Link *);
extern void InsertLLE(List *,Link *,Link *);
static Packet *packet_alloc(void) {
    Packet *p;
    p = (Packet *)gFreeListeners.head;
    RemoveLLE(&gFreeListeners, &p->link);
    p->type = 0;
    return p;
}

static void packet_post(Packet *p) {
    InsertLLE(&gActiveListeners, &p->link, gActiveListeners.head);
    p->ready = 1;
}

void audio_lock(void);

void audio_unlock(void);

Packet *AudAddListener(Vec3 *a,Vec3 *b,Vec3 *c,Vec3 *d) {
    Packet *packet;
    if(!gListeners) return (Packet *)-1;
    audio_lock();
    if (gFreeListeners.head) {}
    packet = packet_alloc();
    packet->a.x=a->x;
    packet->a.y=a->y;
    packet->a.z=a->z;
    packet->b.x=b->x;
    packet->b.y=b->y;
    packet->b.z=b->z;
    packet->c.x=c->x;
    packet->c.y=c->y;
    packet->c.z=c->z;
    packet->d.x=d->x;
    packet->d.y=d->y;
    packet->d.z=d->z;
    packet_post(packet);
    audio_unlock();
    return packet;
}

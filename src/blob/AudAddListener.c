#include "game/game.h"

typedef struct Link {struct Link *next,*prev;} Link;
typedef struct List {u8 indirect,doubly,pad[2];u32 count;Link *head,*tail;} List;
typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Packet {Link link;u8 type,ready,opaque[2];Vec3 a,b,c,d;} Packet;

VIEW(List, gFreeListeners);
#define gFreeListeners gFreeListeners_view
VIEW(List, gActiveListeners);
#define gActiveListeners gActiveListeners_view

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

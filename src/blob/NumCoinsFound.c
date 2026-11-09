#include "game/game.h"

typedef struct Resource { u8 *data; } Resource;
typedef struct Object {void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;
typedef struct Player {u8 first;u8 selector;u8 opaque[70];Handle *handle;} Player;
typedef struct Record96 {u8 opaque[92];u16 bits;u8 tail[2];} Record96;
typedef struct Record64 {u8 opaque[60];u16 bits;u8 tail[2];} Record64;

extern Player gPlayers[];
VIEW(Object *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view

u8 NumCoinsFound(int index,int lane)
{
    int mode=gTrackNum;
    u32 bits;
    if(mode>=0 && mode<6) {
        Player *player=&gPlayers[index];
        Resource *resource;
        Record96 *record;
        if(player->handle==0)player->handle=(Handle *)&gJustPlayers[player->selector];
        resource=player->handle->object->resource;
        if(resource==0)return 16;
        record=(Record96 *)(resource->data+140)+mode;
        bits=record->bits;
    } else if(mode>=14 && mode<18) {
        Player *player=&gPlayers[index];
        Record64 *record;
        if(player->handle==0)player->handle=(Handle *)&gJustPlayers[player->selector];
        record=(Record64 *)(player->handle->object->resource->data+396)+mode;
        bits=record->bits;
    } else bits=0;
    if(lane!=0) {
        u32 mask;
        if(lane==1)mask=0xff00;
        else mask=0xff;
        bits&=mask;
    }
    return (u8)CountBits(bits,16);
}

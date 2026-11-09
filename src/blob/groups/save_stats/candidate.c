#include "game/game.h"

typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev;void *lookup;u8 other12[32];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;

extern void PS_WriteFile(void *,void *,u32);
void SavePlayerStats(Handle *handle,u8 mode)
{
    u8 *data=handle->object->resource->data,*record;
    u32 *hash;
    if((mode>=0 && mode<6) || (mode>=19 && mode<25)) {
        if(mode>=19) { record=data; record+=mode*96; record-=1108; }
        else { record=data; record+=mode*96; record+=140; }
        *(u32 *)record=CalcChecksumInternal(record+4,92);
        PS_WriteFile(handle->object->lookup,(u32 *)record,96);
    } else if(mode>=6 && mode<14) {
        record=data; record+=mode*12;
        hash=(u32 *)(record+1476);
        *hash=CalcChecksumInternal(record+1480,8);
        PS_WriteFile(handle->object->lookup,hash,12);
    } else if(mode>=14 && mode<18) {
        record=data; record+=mode*64;
        hash=(u32 *)(record+396);
        *hash=CalcChecksumInternal(record+400,60);
        PS_WriteFile(handle->object->lookup,hash,64);
    } else {
        record=data; record+=mode*24;
        hash=(u32 *)(record+1212);
        *hash=CalcChecksumInternal(record+1216,20);
        PS_WriteFile(handle->object->lookup,hash,24);
    }
}

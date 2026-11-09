#include "game/game.h"

typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev;void *lookup;u8 other12[32];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;

extern void PS_WriteFile(void *,u32 *,int);

void SavePlayerCircuitStats(Handle *handle,u8 kind)
{
    u8 *record=handle->object->resource->data+kind*28+1668;
    *(u32 *)record=CalcChecksumInternal(record+4,24);
    PS_WriteFile(handle->object->lookup,(u32 *)record,28);
}

void SavePlayerCircuit(Handle *handle)
{
    u8 *record=handle->object->resource->data+1780;
    *(u32 *)record=CalcChecksumInternal(record+4,72);
    PS_WriteFile(handle->object->lookup,(u32 *)record,76);
}

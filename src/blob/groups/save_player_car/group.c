#include "game/game.h"

typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev;void *lookup;u8 other12[32];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;

extern void PS_WriteFile(void *,u32 *,int);

void SavePlayerCar(Handle *handle,u8 value)
{
    u8 *data=handle->object->resource->data;
    switch(data[71]) {
    case 0:case 1:case 2:case 3:
        if(value==data[77])return;
        data[77]=value;
        *(u32 *)(data+72)=CalcChecksumInternal(data+76,16);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+72),20);
        break;
    case 6:
        if(value==data[97])return;
        data[97]=value;
        *(u32 *)(data+92)=CalcChecksumInternal(data+96,12);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+92),16);
        break;
    case 4:
        if(value==data[113])return;
        data[113]=value;
        *(u32 *)(data+108)=CalcChecksumInternal(data+112,16);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+108),20);
        break;
    case 5:
        if(value==data[133])return;
        data[133]=value;
        *(u32 *)(data+128)=CalcChecksumInternal(data+132,8);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+128),12);
        break;
    }
}

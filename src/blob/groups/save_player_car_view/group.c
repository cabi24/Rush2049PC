#include "game/game.h"

typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev;void *lookup;u8 other12[32];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;

extern void PS_WriteFile(void *,u32 *,int);

void SavePlayerCarView(Handle *handle,u8 value)
{
    u8 *data=handle->object->resource->data;
    switch(data[71]) {
    case 0:case 1:case 2:case 3:
        if(value==data[79])return;
        data[79]=value;
        *(u32 *)(data+72)=CalcChecksumInternal(data+76,16);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+72),20);
        break;
    case 6:
        if(value==data[99])return;
        data[99]=value;
        *(u32 *)(data+92)=CalcChecksumInternal(data+96,12);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+92),16);
        break;
    case 4:
        if(value==data[115])return;
        data[115]=value;
        *(u32 *)(data+108)=CalcChecksumInternal(data+112,16);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+108),20);
        break;
    case 5:
        if(value==data[135])return;
        data[135]=value;
        *(u32 *)(data+128)=CalcChecksumInternal(data+132,8);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+128),12);
        break;
    }
}

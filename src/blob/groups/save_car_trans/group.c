/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef unsigned int u32;
typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev;void *lookup;u8 other12[32];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;
extern u32 CalcChecksumInternal(u8 *,u32);
extern void PS_WriteFile(void *,u32 *,int);
u32 CalcChecksumInternal(u8 *data,u32 size);

void SavePlayerCarTrans(Handle *handle,u8 value)
{
    u8 *data=handle->object->resource->data;
    switch(data[71]) {
    case 0:case 1:case 2:case 3:
        if(value==data[78])return;
        data[78]=value;
        *(u32 *)(data+72)=CalcChecksumInternal(data+76,16);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+72),20);
        break;
    case 6:
        if(value==data[98])return;
        data[98]=value;
        *(u32 *)(data+92)=CalcChecksumInternal(data+96,12);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+92),16);
        break;
    case 4:
        if(value==data[114])return;
        data[114]=value;
        *(u32 *)(data+108)=CalcChecksumInternal(data+112,16);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+108),20);
        break;
    case 5:
        if(value==data[134])return;
        data[134]=value;
        *(u32 *)(data+128)=CalcChecksumInternal(data+132,8);
        PS_WriteFile(handle->object->lookup,(u32 *)(data+128),12);
        break;
    }
}

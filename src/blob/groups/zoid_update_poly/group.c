#include "game/game.h"

typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Vertex20 {float x,y,z;s16 s,t;u32 color;} Vertex20;
typedef struct Record88 {s16 count;u16 flags,parameter,unknown;Vertex20 vertices[4];} Record88;
typedef struct Texture36 {u8 prefix[16];u16 width,height;u8 middle[8];u32 flags;u32 tail;} Texture36;
typedef struct TextureList8 {Texture36 *entries;int count;} TextureList8;
VIEW(TextureList8[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view

void ZOID_UpdatePoly(Record88 *record,int count,Vec3 *positions,u16 parameter,u8 *color,u16 flags,int indexed)
{
    Texture36 *texture;
    int i,corner;
    if(!count)count=record->count;
    else record->count=count;
    if(flags)record->flags=flags;
    if(indexed) {
        record->parameter=parameter;
        record->flags|=0x400;
    }
    if(record->flags&0x400) {
        texture=&gTexDefPtrs[record->parameter>>10].entries[record->parameter&0x3FF];
    } else texture=0;
    for(i=0;i<count;i++) {
        if(texture) {
            corner=i;
            if(gMirrorWaves&&(record->flags&0x80))corner=i+4;
            switch(corner) {
            case 0:
                if(texture->flags&0x10000)record->vertices[i].s=texture->width<<5;
                else record->vertices[i].s=(texture->width<<6)-16;
                if(texture->flags&0x20000)record->vertices[i].t=texture->height<<5;
                else record->vertices[i].t=(texture->height<<6)-16;
                break;
            case 1:
                if(texture->flags&0x10000)record->vertices[i].s=0;
                else record->vertices[i].s=(texture->width<<5)-16;
                if(texture->flags&0x20000)record->vertices[i].t=texture->height<<5;
                else record->vertices[i].t=(texture->height<<6)-16;
                break;
            case 2:
                if(texture->flags&0x10000)record->vertices[i].s=0;
                else record->vertices[i].s=(texture->width<<5)-16;
                if(texture->flags&0x20000)record->vertices[i].t=0;
                else record->vertices[i].t=(texture->height<<5)-16;
                break;
            case 3:
                if(texture->flags&0x10000)record->vertices[i].s=texture->width<<5;
                else record->vertices[i].s=(texture->width<<6)-16;
                if(texture->flags&0x20000)record->vertices[i].t=0;
                else record->vertices[i].t=(texture->height<<5)-16;
                break;
            case 4:
                if(texture->flags&0x10000)record->vertices[i].s=0;
                else record->vertices[i].s=(texture->width<<5)-16;
                if(texture->flags&0x20000)record->vertices[i].t=texture->height<<5;
                else record->vertices[i].t=(texture->height<<6)-16;
                break;
            case 5:
                if(texture->flags&0x10000)record->vertices[i].s=texture->width<<5;
                else record->vertices[i].s=(texture->width<<6)-16;
                if(texture->flags&0x20000)record->vertices[i].t=texture->height<<5;
                else record->vertices[i].t=(texture->height<<6)-16;
                break;
            case 6:
                if(texture->flags&0x10000)record->vertices[i].s=texture->width<<5;
                else record->vertices[i].s=(texture->width<<6)-16;
                if(texture->flags&0x20000)record->vertices[i].t=0;
                else record->vertices[i].t=(texture->height<<5)-16;
                break;
            case 7:
                if(texture->flags&0x10000)record->vertices[i].s=0;
                else record->vertices[i].s=(texture->width<<5)-16;
                if(texture->flags&0x20000)record->vertices[i].t=0;
                else record->vertices[i].t=(texture->height<<5)-16;
                break;
            }
        }
        if(positions) {
            if(record->flags&0x4000) {
                record->vertices[i].x=positions[i].x;
                record->vertices[i].y=positions[i].y;
            } else {
                record->vertices[i].x=positions[i].x*16.0f;
                record->vertices[i].y=positions[i].y*16.0f;
                record->vertices[i].z=positions[i].z*16.0f;
            }
        }
        if(color)record->vertices[i].color=*(u32 *)color;
    }
}

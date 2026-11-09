#include "game/game.h"

typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev; void *fileh; u8 other_C[0x2C - 0xC];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;
typedef struct Player76 {u8 index,selector;u8 other2[62];u16 half64;u8 other66[6];Handle *handle;} Player76;
typedef struct {u32 word0,maximum,rate,total;u16 frames,extra;u32 counters[10];u32 other60;} Stats64;
typedef struct {u8 other0[8];u32 maximum;u8 other12[8];s32 total;u8 other24[48];s16 counters[10];u8 other92[28];} Live120;
extern Player76 gPlayers[];

VIEW(Object *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(Stats64[], gTotalStuntStats);
#define gTotalStuntStats gTotalStuntStats_view
VIEW(Live120[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view

void RecordStunt(void)
{
    s32 player,which,j;
    s32 offset;
    s32 selector=gTrackNum-14;
    Player76 *record;
    Live120 *live;
    Stats64 *stats;
    u32 value;
    for(player=0;player<gNumPlayers;player++) {
        record=&gPlayers[player];
        offset=selector*64;
        if(record->handle==0) record->handle=(Handle *)&gJustPlayers[record->selector];
        if(record->handle->object->resource==0) return;
        live=&gStuntPlayer[player];
        for(which=0;which<2;which++) {
            if(which==0) stats=(Stats64 *)(record->handle->object->resource->data+offset+1292);
            else stats=(Stats64 *)((u8 *)gTotalStuntStats+offset);
            if(stats->maximum<live->maximum) stats->maximum=live->maximum;
            value=live->total/gStuntModeMins;
            if(stats->rate<value) stats->rate=value;
            stats->total+=live->total;
            stats->frames+=gStuntModeMins;
            stats->extra+=record->half64;
            for(j=0;j<10;j++) stats->counters[j]+=live->counters[j];
        }
        SavePlayerStats(record->handle,(u8)gTrackNum);
    }
}

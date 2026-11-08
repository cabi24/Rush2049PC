/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef short s16;
typedef unsigned int u32;
typedef struct Resource Resource;
typedef struct Object { void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player { u8 opaque[72]; Handle *handle; } Player;
extern u32 gPauseState;
extern void *pause_DimBlit,*pause_TitleBlit,*pause_PauseBlit,*pause_SureBlit,*pause_RumbleBlit;
extern uintptr_t pause_gTitlePane;
extern Player gPlayers[];
extern s16 gNumPlayers;
extern void RemoveBlit(void *);
extern void SaveSelectPlayer(Handle *);
extern void RemovePanes(int);
void updatePauseStateCleanup(void)
{
    int i;
    Player *player;
    switch(gPauseState) {
    case 0:
        if(pause_DimBlit) {
            RemoveBlit(pause_DimBlit);
            pause_DimBlit=0;
        }
        player=gPlayers;
        for(i=0;i<gNumPlayers;i++,player++) {
            if(player->handle->object->resource)SaveSelectPlayer(player->handle);
        }
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        if(pause_TitleBlit) {RemoveBlit(pause_TitleBlit);pause_TitleBlit=0;}
        if(pause_PauseBlit) {RemoveBlit(pause_PauseBlit);pause_PauseBlit=0;}
        if(pause_SureBlit) {RemoveBlit(pause_SureBlit);pause_SureBlit=0;}
        if(pause_RumbleBlit) {RemoveBlit(pause_RumbleBlit);pause_RumbleBlit=0;}
        break;
    }
    RemovePanes(0);
    pause_gTitlePane=0;
}

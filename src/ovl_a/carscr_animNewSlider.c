/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Complete image-A A3DA4 projected gauge callback, 912 bytes.
 * Standalone source. No caller, inlined helper, deleted-static stub or own data.
 * Original carscr animNewSlider evidence establishes exactly type, p, opt and
 * signed-short screenPos[2], with no declared slot or quarter-height cache.
 * Native-specific button offsets, tire indices, geometry and arithmetic remain.
 * Hidden returns int in the original contract; all used native blit offsets
 * and the 64-byte record are checked separately. No extra local storage.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID; intptr_t AnimDTA; u16 BLIdx, reserved36; void *data, *child;
} Blit;
/* PC: the record as cleanup_colortrace's Slot has it (kind, handle, angle, matrix, position, then the alpha byte),
 * which the PC loader then fills element by element. Seen as matrix[16] it would swap the alpha byte with the
 * three after it as one f32 (docs/port/ENDIAN.md). */
typedef union MenuSlot {
    float matrix[16];
    struct { s32 x,y; float depth; float rest[12]; u8 alpha; u8 tail[3]; } state;
} MenuSlot;
typedef struct ProjectContext {
    float rotation[3][3], position[3]; u8 opaque[104];
} ProjectContext;
extern s16 gNumPlayers;
extern s8 gCarLocked[], gThisCar[];
extern MenuSlot carscr_buttonInfo[][44];
extern float carscr_slider_hide_angle_min, carscr_slider_hide_angle_max, carscr_slider_flip_angle, carscr_front_tire_scale_step, carscr_rear_tire_scale_step;
extern float gFrontTireScale_players[][13], gRearTireScale_players[][13];
extern ProjectContext gCamera[];
extern s32 Hidden(Blit *,s32);
extern void ZOID_ProjectWindow(s32,float *,ProjectContext *,float *,s16 *);
extern void UpdateBlit(Blit *);
s32 carscr_animNewSlider(Blit *blt)
{
    s32 type=blt->AnimID&15;
    s32 p=(blt->AnimID>>4)&15;
    s32 opt=blt->AnimID>>16;
    s16 screenPos[2];
    if (p>=gNumPlayers) {
        Hidden(blt,1);
        blt->AnimFunc=0;
        return 1;
    }
    if (gCarLocked[p]) {
        Hidden(blt,1);
        return 1;
    }
    if (Hidden(blt,
        (carscr_slider_hide_angle_min<carscr_buttonInfo[p][opt].state.depth && carscr_buttonInfo[p][opt].state.depth<carscr_slider_hide_angle_max) || !carscr_buttonInfo[p][opt].state.alpha))
        return 1;
    ZOID_ProjectWindow(p,&carscr_buttonInfo[p][opt+19].matrix[12],&gCamera[p],0,screenPos);
    blt->X=screenPos[0]-blt->Width/2;
    blt->Y=screenPos[1]-(blt->Height/4)/2;
    blt->Left=0;
    blt->Right=blt->Width-1;
    blt->Top=0;
    blt->Bot=(blt->Height/4)-1;
    blt->Alpha=carscr_buttonInfo[p][opt].state.alpha;
    if (carscr_slider_flip_angle < carscr_buttonInfo[p][opt].state.depth) {
        blt->Top += blt->Height/2;
        blt->Bot += blt->Height/2;
    }
    if (type==1) {
        blt->Top+=(blt->Height/4);
        blt->Bot+=(blt->Height/4);
        switch (opt) {
        case 13:
            blt->Right=(gFrontTireScale_players[p][gThisCar[p]]-.75f)/carscr_front_tire_scale_step*(blt->Width-1)/15.0f;
            break;
        case 14:
            blt->Right=(gRearTireScale_players[p][gThisCar[p]]-.75f)/carscr_rear_tire_scale_step*(blt->Width-1)/15.0f;
            break;
        }
    }
    UpdateBlit(blt);
    return 1;
}

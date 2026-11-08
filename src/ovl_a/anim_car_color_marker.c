/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image A:803A3A6C..803A3D9C; full select-screen marker color callback.
 * Native-authoritative reconstruction. Projection-reaching descriptors must
 * identify player 0..3 and marker 7..9. Earlier hide-only exits are wider.
 * This is a 64-byte slot view, not a claim of recovered original N64 types.
 * Each player has a native 2816-byte stride, 44 slots. Projection observes
 * three floats at slot +19, float +12; visibility is +8/+60 in this slot.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID;
} Blit;
/* PC: the record as cleanup_colortrace's Slot has it (kind, handle, angle, matrix, position, then the alpha byte),
 * which the PC loader then fills element by element. Seen as matrix[16] it would swap the alpha byte with the
 * three after it as one f32 (docs/port/ENDIAN.md). */
typedef union MenuSlot {
    float matrix[16];
    struct { s32 x,y; float depth; float rest[12]; u8 alpha; u8 tail[3]; } state;
} MenuSlot;
typedef struct Color { u8 r,g,b,a; } Color;
typedef struct ProjectContext {
    float rotation[3][3], position[3]; u8 opaque[104];
} ProjectContext;
extern s16 gNumPlayers;
extern s8 gCarLocked[], gThisCar[];
extern MenuSlot carscr_buttonInfo[][44];
extern float carscr_marker_hide_angle_min, carscr_marker_hide_angle_max;
extern ProjectContext gCamera[];
/* The car colour settings, s8[5][13] by row (player + 1; row 0 the defaults) and car: palette indexes
 * (paint_colors_rgb). */
extern s8 car_color_choice_a[],car_color_choice_b[],car_color_choice_c[];
extern Color carscr_marker_colors[][3],paint_colors_rgb[];
extern s8 Hidden(Blit *,s32);
extern void ZOID_ProjectWindow(s32,float *,ProjectContext *,void *,s16 *);
extern void UpdateBlit(Blit *);
s32 anim_car_color_marker(Blit *blt)
{
    s8 (*color_a)[13] = (s8 (*)[13]) car_color_choice_a;
    s8 (*color_b)[13] = (s8 (*)[13]) car_color_choice_b;
    s8 (*color_c)[13] = (s8 (*)[13]) car_color_choice_c;
    s32 player=blt->AnimID&15;
    s32 marker=(blt->AnimID>>16)&255;
    MenuSlot *slot;
    s32 palette;
    s16 position[2];
    s32 channel;

    if (player>=gNumPlayers) {
        Hidden(blt,1);
        blt->AnimFunc=0;
        return 1;
    }
    if (gCarLocked[player]) {
        Hidden(blt,1);
        return 1;
    }
    slot=&carscr_buttonInfo[player][marker];
    if (Hidden(blt,
        (carscr_marker_hide_angle_min<slot->state.depth && slot->state.depth<carscr_marker_hide_angle_max) || !slot->state.alpha))
        return 1;
    ZOID_ProjectWindow(player,&slot[19].matrix[12],&gCamera[player],0,position);
    blt->X=position[0]-blt->Width/2;
    blt->Y=position[1]-blt->Height/2;
    blt->Alpha=slot->state.alpha;
    if (marker==7) {
        palette=color_a[player + 1][gThisCar[player]];
        channel=0;
    } else if (marker==8) {
        palette=color_b[player + 1][gThisCar[player]];
        channel=1;
    } else if (marker==9) {
        palette=color_c[player + 1][gThisCar[player]];
        channel=2;
    }
    carscr_marker_colors[player][channel].r=paint_colors_rgb[palette].r;
    carscr_marker_colors[player][channel].g=paint_colors_rgb[palette].g;
    carscr_marker_colors[player][channel].b=paint_colors_rgb[palette].b;
    carscr_marker_colors[player][channel].a=blt->Alpha;
    blt->Image=&carscr_marker_colors[player][channel];
    blt->AnimID=(blt->AnimID&0x0fffffff)|((u32)palette<<28);
    UpdateBlit(blt);
    return 1;
}

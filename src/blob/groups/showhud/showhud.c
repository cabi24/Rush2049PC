/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * skid_mark_render (0x800F0100, 1,396 B; historical label) = MAT3 ShowHUD@hud.c (GC 0x8001C980, 904 B;
 * reference/repos/mat3/decomp/src/GameSource/src/hud.c). Creates the HUD blit sets on first call (clears the
 * per-player checkpoint/lap/elapsed timers and coin records, then NewMultiBlit = sound_control: overlay set for
 * attract/credits, 2/3-player divider sets, ShowHUDExtra (image B 0x803925D0) for battle/stunt or the race set,
 * then the HUD set), and in attract mode with credits on cycles the credit panes (SetCountdownTimer =
 * viScheduleTick, GetCountdownTimeRemaining = viGetTimeToDeadline, Random = func_8008B2E4 inlined, HAL_SetFont,
 * HAL_GetTextWidth = object_manager_update, RemovePanes = sound_handles_clear, InitPane = ambient_sound_set).
 * N64 constants differ from the GC (race/HUD blit counts 89/8, credit delay /13, bottom row 145; the 2/3-player
 * sets only when D_80156994 || D_8014978C >= 6).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * function-local statics delay/nextSwitch, credit-name for-loop, x-before-y corner order), unchanged except
 * one lever: HAL_SetFont is inlined (as in retail: osRecvMesg + slot_state_setup(s2=11) + osSendMesg)
 * through non-static copies HAL_SetFont_ShowHUD/_GetSemaphore_ShowHUD/_ReleaseSemaphore_ShowHUD defined after the
 * caller (not on keep, the locked object_create is kept and stays a jal in the unit, 335/349 words off).
 * Shaping (disclosed): _ReleaseSemaphore_ShowHUD and HAL_SetFont_ShowHUD are written on ONE source line. as1 breaks
 * ready-list ties by source line; in the group compile the inlined bodies keep their own .loc lines, and retail's
 * order (addiu a0 / move s0,v0 / move a1 before osSendMesg) needs the osSendMesg and `old = slot_state_setup()`
 * statements on the same line (the whole-program unit gives every inlined instruction the call-site line, where
 * the placement does not matter: unit EQUAL either way; group 2 words swapped otherwise).
 * gSeconds is `extern volatile float` as in MAT3 rush_protos.h (D_8002EB90).
 * The block-local loop index `i` is a reconstruction (absent from the DWARF local list of ShowHUD).
 * MATCH: whole-program unit EQUAL (the whole-program unit --tag); group showhud (the scorer) MATCH.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Blit Voice;
typedef struct Pane Pane;
typedef s32 (*PaneAnimation)(Voice *);
typedef struct MultiBlit {
    const char *texname; s16 dulx,duly,width,height,top,bot,left,right;
    u32 zdepth,alpha; s32 (*animfunc)(Voice *); u32 animid;
} MultiBlit;
typedef struct RecordSlot { s16 id; u8 pad02[50]; } RecordSlot;   /* 52 bytes */
typedef struct RecordRow { RecordSlot a, b; } RecordRow;              /* 104 bytes */
/* Complete native resource-table layout, shared with its actual loader. */
#ifdef TARGET_PC
/* PC: gText's pointer sections are native char * tables (port/assets/text.c) */
typedef struct ModelTables {
    u16 count0; char **ptrs0; u16 count1,count2; u16 *data1; char **ptrs2;
    u16 count3,count4,count5; u16 *data3,*data4; char **ptrs5;
} ModelTables;
#else
typedef struct ModelTables {
    u16 count0; u32 *ptrs0; u16 count1,count2; u16 *data1; u32 *ptrs2;
    u16 count3,count4,count5; u16 *data3,*data4; u32 *ptrs5;
} ModelTables;
#endif

extern Voice *D_80154198;
extern Voice *D_801541A0;
extern Voice *D_801541A4;
extern RecordRow D_801541A8[];
extern f32 D_80142740[4], D_80142770[4], D_80142AC8[4];
extern u32 D_801174B4;
extern s8 D_80156994;
extern s8 D_8014978C;
extern s16 D_80151AD0;
extern s32 gameplay_mode;
extern MultiBlit D_80115A44[], D_801159FC[], D_80115A20[], D_80114C58[], D_801158DC[];
extern s16 D_80142D70;
extern s32 D_80138660;
extern s32 D_80154394;
extern volatile f32 D_8002EB90;
extern s32 D_801543A0;
extern s32 D_801543A8;
extern s32 D_801543B0;
extern char *D_80114A0C[][11];
extern ModelTables D_8017A4E0;

Voice *sound_control(s16, s16, const MultiBlit *, s16);
Pane *ambient_sound_set(s32,s32,s32,s32,s32,PaneAnimation,u32,s32);
void sound_handles_clear(s32 arg0);
void viScheduleTick(f32);
f32 viGetTimeToDeadline(void);
void func_803925D0(void);
typedef struct { u32 opaque[6]; } OSMesgQueue;
extern OSMesgQueue D_801461D0;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 slot_state_setup(s32 fontID);
void _GetSemaphore_ShowHUD(void);
void _ReleaseSemaphore_ShowHUD(void);
s32 HAL_SetFont_ShowHUD(s32 fontID);
extern u32 object_manager_update(void *, s16);

extern f32 func_8008B2E4(f32);

#ifdef TARGET_PC
/* PC: the race set with a minimap dot and a radar blip for each car past the N64's 8 (docs/port/MORECARS.md 7). The
 * set (D_80114C58, ROM data) has a dot for each of the HUD order's first 8 entries (AnimateDot, func_80109A60: AnimID
 * the entry) and, for each of the 4 viewers, a blip for each of car slots 0-7 (AnimateTraffic, func_80108F40: AnimID
 * bits 8-11 the viewer, 0-3 the slot), each run of them ending at 7. With more than 8 cars the humans, last in the
 * HUD order, had no dot, and slots 8-11 no blip. So a race of more than 8 cars gets a copy of the set, made at run
 * time, with each run continued after its 7 by copies of that entry for 8 to the race's cars less one; a race of 8 or
 * fewer gets the set itself, as the N64 does. */
#define PC_HUD_RACE_ROM 89
#define PC_HUD_RACE_RUNS 8   /* room for 8 runs to grow: the set has 5 */
#define PC_HUD_RACE_MAX (PC_HUD_RACE_ROM + PC_HUD_RACE_RUNS * (PC_MAX_LINKS - 8))
static MultiBlit pc_hud_race[PC_HUD_RACE_MAX];
static s16 pc_hud_race_count;
s32 func_80109A60(Voice *);   /* AnimateDot */
s32 func_80108F40(Voice *);   /* AnimateTraffic */

static void pc_hud_race_build(void)
{
    s32 cars = PC_RACE_CARS, i, k, n = 0;

    for (i = 0; i < PC_HUD_RACE_ROM; i++) {
        const MultiBlit *e = &D_80114C58[i];
        pc_hud_race[n++] = *e;
        if ((e->animfunc == func_80109A60 || e->animfunc == func_80108F40) && (e->animid & 0xF) == 7) {
            for (k = 8; k < cars && n < PC_HUD_RACE_MAX - (PC_HUD_RACE_ROM - 1 - i); k++) {
                pc_hud_race[n] = *e;
                pc_hud_race[n].animid = (e->animid & ~0xFu) | (u32)k;
                n++;
            }
        }
    }
    pc_hud_race_count = n;
}

static const MultiBlit *pc_hud_race_set(void)
{
    if (PC_RACE_CARS <= 8) {
        return D_80114C58;
    }
    pc_hud_race_build();
    return pc_hud_race;
}

static s16 pc_hud_race_n(void)
{
    if (PC_RACE_CARS <= 8) {
        return PC_HUD_RACE_ROM;
    }
    pc_hud_race_build();
    return pc_hud_race_count;
}
#define PC_HUD_RACE_SET pc_hud_race_set()
#define PC_HUD_RACE_N pc_hud_race_n()
#else
#define PC_HUD_RACE_SET D_80114C58
#define PC_HUD_RACE_N 89
#endif

void skid_mark_render(void) {
    s32 created = 0;
    s32 x;
    s32 y;
    static f32 delay;
    static f32 nextSwitch;
    if (D_80154198 == 0) {
        s32 i; /* Reconstruction-only loop index; absent from original named-local list. */
        for (i = 0; i < 4; i++) {
            D_80142740[i] = 0.0f;
            D_80142770[i] = 0.0f;
            D_80142AC8[i] = 0.0f;
            D_801541A8[i].a.id = -1;
            D_801541A8[i].b.id = -1;
        }
        if (D_801174B4 & 0x02000000) {
            D_80154198 = sound_control(0, 0, D_80115A44, 1);
        } else if (D_801174B4 & 8) {
            D_80154198 = sound_control(0, 0, D_80115A44, 1);
        } else {
            if (D_80156994 || D_8014978C >= 6) {
                if (D_80151AD0 == 2) {
                    D_801541A4 = sound_control(0, 0, D_801159FC, 1);
                } else if (D_80151AD0 == 3) {
                    D_801541A4 = sound_control(0, 0, D_80115A20, 1);
                }
            }
            if (gameplay_mode == 6 || gameplay_mode == 4) {
                func_803925D0();
            } else {
                D_801541A0 = sound_control(0, 0, PC_HUD_RACE_SET, PC_HUD_RACE_N);
            }
            D_80154198 = sound_control(0, 0, D_801158DC, 8);
        }
        D_80142D70 = 0;
        created = 1;
    }
    if ((D_801174B4 & 8) && D_80138660) {
        if (created) {
            D_80154394 = -1;
            viScheduleTick(90.0f);
            delay = viGetTimeToDeadline() / 13.0f;
            nextSwitch = D_8002EB90;
        }
        if (nextSwitch <= D_8002EB90) {
            nextSwitch += delay;
            D_80154394++;
            D_801543A0 = func_8008B2E4(4.0f);
            HAL_SetFont_ShowHUD(11);
            D_801543A8 = object_manager_update((void *)D_8017A4E0.ptrs2[D_8017A4E0.data1[0] + D_80154394], -1);
            for (D_801543B0 = 0; D_80114A0C[D_80154394][D_801543B0]; D_801543B0++) {
                if (D_801543A8 < object_manager_update(D_80114A0C[D_80154394][D_801543B0], -1)) {
                    D_801543A8 = object_manager_update(D_80114A0C[D_80154394][D_801543B0], -1);
                }
            }
            switch (D_801543A0) {
            case 0:
                x = 20;
                y = 20;
                break;
            case 1:
                x = 280 - D_801543A8;
                y = 20;
                break;
            case 2:
                x = 20;
                y = 145 - D_801543B0 * 10;
                break;
            default:
                x = 280 - D_801543A8;
                y = 145 - D_801543B0 * 10;
                break;
            }
            sound_handles_clear(0);
            ambient_sound_set(x, y, x + D_801543A8 + 20, D_801543B0 * 10 + y + 32, 192, 0, 0, 0);
        }
    }
}

void _GetSemaphore_ShowHUD(void) { osRecvMesg(&D_801461D0, (void **)0, 1); }
void _ReleaseSemaphore_ShowHUD(void) { osSendMesg(&D_801461D0, (void *)0, 0); } s32 HAL_SetFont_ShowHUD(s32 fontID) { s32 old; _GetSemaphore_ShowHUD(); old = slot_state_setup(fontID); _ReleaseSemaphore_ShowHUD(); return old; }

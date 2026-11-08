/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: func_803949CC = uninitScreen@playerscr.c (MAT3 GC uninitScreen) now calls
 * uninitButtons(signed char p) (MAT3 playerscr.c) as the non-kept helper func_803949C4, defined after its caller (MAT3
 * file order); its deleted procedure is the retail stub 0x803949C4 right before uninitScreen. The button loop moved
 * verbatim apart from MAT3's direct indexing (the earlier `player`/`slot`/`j` carriers are gone; the s8 parameter is
 * MAT3's `signed char p` and is what keeps the code identical: an s32 parameter gives 71/77). Context unchanged. */
typedef signed char s8;typedef unsigned char u8;
typedef short s16;typedef unsigned short u16;typedef int s32;typedef unsigned int u32;typedef float f32;
extern s16 D_8014A108;
extern s8 D_803BADBC[],D_803BADC0[],D_803BAD78[];
extern intptr_t D_803BAC68[];
extern s32 D_803BAC20[], D_803BACE0[];
extern s8 D_803BAD00[][14],D_803BAD40[][14];

typedef struct Owner Owner;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Item {void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 player;} Item;
typedef struct Selection {Item *item;} Selection;
typedef struct OwnerBody {Owner *next,*previous;Selection *selection;} OwnerBody;
struct Owner {OwnerBody *body;};
typedef struct Player {u8 unaccessed0,id;u16 unaccessed2;u32 flags;u8 unaccessed8[64];Owner *owner;} Player;
typedef struct Blit Blit;typedef struct FiveParts FiveParts;
typedef struct MultiBlit {char *name;s16 x,y,width,height,top,bottom,left,right;u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;} MultiBlit;
extern Player D_8014A118[];
extern u8 D_8012E67C[];
extern s32 D_803BAD80[],D_803B5CFC[][24];
extern s8 D_803BACF4,D_803B5ED0;
extern s16 D_80151AD0;
extern f32 D_803B5D0C,D_803B5D10,D_803B5D14,D_803B5D18,D_803B5D1C,D_803B5D20,D_803B5D24,D_803B5D28;
extern FiveParts *D_803BAC08[];
extern MultiBlit D_803B5D2C[];
extern Blit *D_803B5ED8;
extern s32 D_803B5ED4;
extern void func_800A3424(void **,s8);
extern void func_80394834(void);
extern void func_80394B00(s32,s32);
extern void *object_create(s32);
extern s16 object_bytes_sum_global(void);
extern void func_80397AD8(s32);
extern void func_800A6404(void);
extern void particle_lifetime_set(void);
extern FiveParts *ambient_sound_set(s32,s32,s32,s32,s32,s32,s32,s32);
extern Blit *sound_control(s16,s16,const MultiBlit *,s16);
extern void entity_audio_update(void);

extern Owner *D_8012E6E0,*D_803BAC80[];
extern Owner D_80146150[];
extern s8 D_803BADB8[],D_803BADC8[],D_803BADCC[],D_803BADD0[],D_803BADD4[];
extern u8 D_803BAC98[][13];
extern s16 D_803BACD0[],D_803BACD8[];
extern s32 D_803BA888[];
extern void *memset(void *,s32,u32);

void func_80394B00(s32 player,s32 mode);

typedef struct PlayerPresence { s8 present; u8 unknown1[15]; } PlayerPresence;
extern PlayerPresence D_80156CF0[];
extern Owner *D_8012E6E0;
extern s32 D_803BA888[];
extern s8 func_800A1A3C(s32),func_800A35F8(s32),func_80094FC4(s32);
extern u32 func_800A35BC(s32);
extern s32 func_800A3508(s32),func_800A3518(s32);

void func_80394834(void);


typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
extern Slot64 D_803B470C[][22];
extern s32 D_803BA868[];
extern f32 D_80154188;
extern void sound_call_minimal(s16),sound_handles_array_clear(FiveParts *);
extern void render_results_screen(void),sound_stop(Blit *),ambient_sounds_clear(void);
extern void func_800A5A40(void),func_800A5B3C(void);

void func_803949C4(s8);
void func_803949CC(void)
{
    s32 i;
    for(i=0;i<D_8014A108;i++) {
        func_803949C4(i);
        if(D_803BAC08[i]) {
            sound_handles_array_clear(D_803BAC08[i]);
            D_803BAC08[i]=0;
        }
    }
    render_results_screen();
    if(D_803B5ED8) {
        sound_stop(D_803B5ED8);
        D_803B5ED8=0;
    }
    ambient_sounds_clear();
    D_80154188=90.0f;
    D_80151AD0=1;
    func_800A5A40();
    func_800A5B3C();
    D_803B5ED0=0;
}

/* uninitButtons@playerscr.c (MAT3 GC uninitButtons(signed char p)); deleted stub 0x803949C4 */
void func_803949C4(s8 p)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) {
        if (D_803B470C[p][buttonIdx].id != -1) {
            sound_call_minimal(D_803B470C[p][buttonIdx].id);
            D_803B470C[p][buttonIdx].id = -1;
        }
    }
    D_803BA868[p] = 0;
}

extern u32 D_801174BC,D_801174B4;
extern s8 D_8015418C[],D_80157244;
extern void player_mode_set(s32,s32),random_seed_init(s32,s32);
extern void net_state_validate(void),func_800B5570(s32);
extern s8 func_800A361C(s32);
extern void resource_type_select(u32),visual_objects_update(s32);
extern void object_render_cleanup(Owner *),func_800D91A0(void);
extern s32 func_800CCEFC(Owner *);
extern void func_800B4DA4(Owner *);
extern void func_80396FA0(s32),func_80394DD8(s8),func_803968C0(s32),func_80395E8C(s32);


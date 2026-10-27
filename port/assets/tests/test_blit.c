/* port/assets/tests/test_blit.c: the game's own blit and pane code on the PC layouts (docs/port/POINTERS.md 9): the
 * blit pool (InitBlits), NewBlit with its InitBlit and UpdateBlit into the sprite list D_80140BF0 (ZOID_AddBlit and
 * the sprite setters), NewMultiBlit with a callback entry, InitPane and its frame geometry, ChangePaneColor, the
 * per-frame AnimFunc pass (UpdateActiveObjects), RemovePane, sound_handles_clear and DelBlit. Palette, texture,
 * AnimFunc and the pane group in AnimDTA are host pointers and must arrive whole; neighbouring blits must not
 * overlap.
 * game: src/blob/InitBlits.c src/blob/func_800B3704.c src/blob/collision_sound_play.c src/blob/func_800A79F4.c
 * game: src/blob/groups/pad_config/group.c src/blob/Input_InitPadHandlers.c src/blob/Input_SetAnalogBounds.c
 * game: src/blob/input_status_update.c src/blob/Input_SetPadSecondaryFlag.c src/blob/Input_SetPadEnabledFlag.c
 * game: src/blob/sound_stop.c src/blob/sound_control.c src/blob/ambient_sound_set.c src/blob/crowd_cheer_play.c
 * game: src/blob/ChangePaneColor.c src/blob/RemovePane.c src/blob/sound_handles_clear.c src/blob/UpdateActiveObjects.c
 * game: src/blob/func_800A79DC.c
 */
#include <string.h>

#include "support.h"

/* sound_control.c's Blit and particle_knot's Sprite: the layouts every PC view has. */
typedef struct Blit {
    const char *Name;
    void *Image, *Info;
    uint16_t TexIndex;
    int16_t X, Y;
    uint16_t Z;
    int16_t Width, Height;
    uint8_t Alpha, Flip;
    int8_t Hide, Init;
    int16_t Top, Bot, Left, Right, color;
    uint16_t reserved;
    int32_t (*AnimFunc)(struct Blit *);
    uint32_t AnimID;
    intptr_t AnimDTA;
    uint16_t BLIdx, reserved36;
    void *data;
    struct Blit *child;
} Blit;
_Static_assert(sizeof(Blit) == 0x68, "the Blit's host size");
typedef struct {
    void *pal, *tex;
    uint16_t model;
    int16_t x, y;
    uint16_t mode;
    int16_t w, h;
    uint8_t color, flags;
    int8_t hidden;
    uint8_t pad17;
    int16_t t0, s0, t1, s1;
} Sprite;
_Static_assert(sizeof(Sprite) == 0x28, "the sprite's host size");
typedef struct {
    int8_t active;
    uint8_t own, a, b, c, d, e, f, g, h;
    uint8_t other10[2];
    Blit *part[5];
} FiveParts;
typedef struct {
    const char *texname;
    int16_t dulx, duly, width, height, top, bot, left, right;
    uint32_t zdepth, alpha;
    int32_t (*animfunc)(Blit *);
    uint32_t animid;
} MultiBlit;

Sprite D_80140BF0[200];
extern Sprite pad_config[200] __attribute__((alias("D_80140BF0")));
int32_t D_801613AC, D_8013C234;   /* sprites in use, high-water mark */
Blit *D_80149450[200];            /* BlitPtrs */
int32_t D_80149788;               /* NumBlits */
intptr_t game_state_flags;
FiveParts D_80116DE4[16];
uint8_t D_80110664[32];           /* InitBlit's built-in image */
volatile uint8_t gNumModels = 1;

typedef struct {
    char name[16];
    uint16_t width, height;
    uint8_t pad[16];
} TexDef;
static TexDef g_tex = {"TITLE", 64, 24, {0}};
void *func_800B24EC(char *name, int16_t *out, int8_t lo, int8_t hi, int32_t err)
{
    *out = 0x405;
    return strcmp(name, "TITLE") == 0 ? &g_tex : NULL;
}

void InitBlits(void);
Blit *func_800B3704(const char *name, int32_t x, int32_t y, int32_t flags);   /* NewBlit */
Blit *sound_control(int16_t ulx, int16_t uly, const MultiBlit *mblit, int16_t nblits);   /* NewMultiBlit */
void Input_ApplyPadConfig(Blit *blit);                                         /* UpdateBlit */
void sound_stop(Blit *blit);                                                   /* DelBlit */
FiveParts *ambient_sound_set(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t style, intptr_t animfunc,
                             int32_t animid, int32_t own);                     /* InitPane */
void ChangePaneColor(FiveParts *group, uint8_t *c1, uint8_t *c2);
void RemovePane(FiveParts *group);
void sound_handles_clear(int32_t all);
void UpdateActiveObjects(void);
void func_800A79DC(int32_t sprite);

static int g_anim_calls, g_cb_calls;
static Blit *g_anim_last;
static int32_t keep_anim(Blit *b) { g_anim_calls++, g_anim_last = b; return 1; }
static int32_t stop_anim(Blit *b) { g_anim_calls++, g_anim_last = b; return 0; }
static int32_t pane_anim(Blit *b) { return 1; }
static int32_t callback(Blit *b) { g_cb_calls++; return 1; }

int main(void)
{
    Blit *a, *t, *m;
    FiveParts *g;
    Sprite *s;
    uint8_t c1[4] = {10, 20, 30, 200}, c2[4] = {1, 2, 3, 100};
    int i;

    /* The pool: 200 blits that don't overlap. */
    InitBlits();
    CHECK(D_80149788 == 0);
    for (i = 1; i < 200; i++)
        CHECK((uint8_t *)D_80149450[i] - (uint8_t *)D_80149450[i - 1] >= (ptrdiff_t)sizeof(Blit));

    /* NewBlit with no name: the built-in image; with a texture name: its record. UpdateBlit fills the sprite. */
    a = func_800B3704(NULL, 10, 20, 0);
    t = func_800B3704("TITLE", 30, 40, 0);
    CHECK(a == D_80149450[0] && t == D_80149450[1] && D_80149788 == 2);
    CHECK(a->Image == D_80110664 && a->Info == NULL && a->AnimDTA == -1 && a->AnimID == 0xFFFFFFFFu);
    CHECK(a->Init == 1 && a->Alpha == 255 && a->child == NULL && a->AnimFunc == NULL);
    CHECK(t->Info == &g_tex && t->TexIndex == 0x405 && t->Width == 64 && t->Height == 24);
    CHECK(a->BLIdx == 0 && t->BLIdx == 1 && D_801613AC == 2);
    s = &D_80140BF0[t->BLIdx];
    CHECK(s->tex == &g_tex && s->pal == NULL && s->model == 0x405 && s->x == 30 && s->y == 40);
    CHECK(s->w == 64 && s->h == 24 && s->t0 == 0 && s->s0 == 0 && s->t1 == 23 && s->s1 == 63);
    CHECK(s->color == 255 && s->hidden == 0 && (s->flags & 0x84) == 0);
    CHECK(D_80140BF0[a->BLIdx].pal == D_80110664);
    t->Hide = 1;
    t->Flip = 1;
    t->Z = 7;
    Input_ApplyPadConfig(t);
    CHECK(s->hidden == 1 && (s->flags & 4) && s->mode == 7);

    /* NewMultiBlit: a callback entry keeps its function in Info and itself in Image; an animated one runs it. */
    {
        MultiBlit mb[2];
        memset(mb, 0, sizeof(mb));
        mb[0].texname = (const char *)-1;   /* what the data loader must give a -1 name */
        mb[0].width = mb[0].height = -1;
        mb[0].alpha = 255;
        mb[0].animfunc = callback;
        mb[0].animid = 0x80000003u;
        mb[1].texname = "TITLE";
        mb[1].dulx = 5;
        mb[1].width = mb[1].height = -1;
        mb[1].alpha = 128;
        mb[1].zdepth = 3;
        mb[1].animfunc = keep_anim;
        mb[1].animid = 9;
        m = sound_control(100, 50, mb, 2);
        CHECK(m != NULL && m->Info == (void *)callback && m->Image == m && m->AnimID == 3);
        CHECK(D_80140BF0[m->BLIdx].tex == (void *)callback && (D_80140BF0[m->BLIdx].flags & 0x80));
        CHECK(m->child != NULL && m->child->AnimFunc == keep_anim && m->child->X == 105 && g_anim_calls == 1);
        CHECK(D_80140BF0[m->child->BLIdx].color == 128 && D_80140BF0[m->child->BLIdx].mode == 3);
    }

    /* InitPane: five blits, the group in part 0's AnimDTA, the frame geometry in the sprites. */
    g = ambient_sound_set(50, 60, 10, 20, 2, (intptr_t)pane_anim, 77, 1);
    CHECK(g == &D_80116DE4[0] && g->active == 1 && g->own == 1 && g->c == 16 && g->g == 192);
    CHECK(g->part[0]->AnimDTA == (intptr_t)g && g->part[1]->AnimDTA == 0);
    for (i = 0; i < 5; i++) {
        CHECK(g->part[i]->AnimFunc == pane_anim && g->part[i]->AnimID == 77);
        CHECK(g->part[i]->BLIdx == (uint16_t)(4 + i));   /* not overwritten by the group pointer */
    }
    CHECK(g->part[2]->Image == &g->a && g->part[0]->Image == &g->e);
    s = &D_80140BF0[g->part[2]->BLIdx];   /* the middle: one pixel in from the frame */
    CHECK(s->x == 11 && s->y == 21 && s->w == 38 && s->h == 38 && s->pal == &g->a);
    s = &D_80140BF0[g->part[3]->BLIdx];   /* the right edge */
    CHECK(s->x == 49 && s->y == 21 && s->w == 1 && s->h == 38);

    /* ChangePaneColor: the group's two colours, and each part's palette and alpha. */
    ChangePaneColor(g, c1, c2);
    CHECK(g->a == 10 && g->d == 200 && g->e == 1 && g->h == 100);
    CHECK(g->part[2]->Image == &g->a && g->part[2]->Alpha == 200 && g->part[4]->Image == &g->e);
    CHECK(D_80140BF0[g->part[2]->BLIdx].color == 200 && D_80140BF0[g->part[1]->BLIdx].color == 100);

    /* The AnimFunc pass: a blit whose function returns 0 is deleted and its sprite freed. */
    g_anim_calls = 0;
    a->AnimFunc = stop_anim;
    i = D_80149788;
    UpdateActiveObjects();
    CHECK(g_anim_calls == 2 && D_80149788 == i - 1 && D_80140BF0[0].hidden == 2);   /* a and m's child */
    CHECK(g_cb_calls == 0);   /* the callback blit has no AnimFunc */

    /* RemovePane, then sound_handles_clear on a second pane. */
    i = D_80149788;
    RemovePane(g);
    CHECK(g->active == 0 && g->part[0] == NULL && D_80149788 == i - 5);
    g = ambient_sound_set(0, 0, 8, 8, 1, 0, 0, 0);
    CHECK(g == &D_80116DE4[0] && D_80149788 == i);
    sound_handles_clear(0);
    CHECK(g->active == 0 && g->part[4] == NULL && D_80149788 == i - 5);

    /* DelBlit on a chain, and freeing a sprite by index. */
    sound_stop(m);
    CHECK(D_80149788 == 1 && D_80149450[0] == t);
    func_800A79DC(t->BLIdx);
    CHECK(D_80140BF0[t->BLIdx].hidden == 2);
    return TEST_DONE();
}

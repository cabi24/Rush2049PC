/* port/assets/tests/test_zoid.c: the game's own scene-node (ObjInstanceList, D_8012E700) functions on the node's
 * PC layout (docs/port/POINTERS.md 9): MBOX_NewObject and its linking, the ZOID setters and getters, the
 * capture-the-flag palette lookup, unlinking, hiding and the collision walk. The table is defined here as the model
 * draw (particle_knot) reads it; each game file has its own view of it, which must put every field where that
 * view does. Host pointers (game data, overlay A's CompLight, a stack rectangle) must arrive whole, and RDRAM-window
 * addresses (WAD textures) zero-extended.
 * game: src/blob/func_8008E26C.c src/blob/render_mode_select.c src/blob/func_8008E144.c
 * game: src/blob/func_8008FFD0.c src/blob/func_8009002C.c src/blob/func_8008AE10.c src/blob/func_8008AE2C.c
 * game: src/blob/func_8008D870.c src/blob/func_800A79B8.c src/blob/func_800A7830.c src/blob/func_800A785C.c
 * game: src/blob/func_800A7884.c src/blob/func_8008E06C.c src/blob/func_80092BC8.c src/blob/func_8008B3A0.c
 * game: src/blob/func_8008AE48.c src/blob/func_8008AE64.c src/blob/func_8008D6FC.c src/blob/func_8010D85C.c
 * game: src/blob/car_mass_set.c src/blob/entity_spawn_callback.c src/blob/model_data_load.c
 * game: src/blob/track_collision.c src/blob/track_collision_edge.c src/blob/func_80090770.c
 */
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* particle_knot's Node: the layout every PC view of the table has. */
typedef struct {
    uint32_t flags, lights;
    void *obj;
    float scale, w;
    uint16_t model;
    int16_t child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    uint8_t prim[4], env[4];
} Node;
_Static_assert(sizeof(Node) == 0x68, "the node's host size");

Node D_8012E700[600];
int32_t D_80156990, D_801569A8;   /* nodes in use, high-water mark */
int16_t D_8015B254 = -1;          /* the root list */

int16_t func_8008E26C(int32_t id, void *owner, int16_t parent, int32_t flags);   /* MBOX_NewObject */
int16_t func_8008E144(int16_t node);
int16_t func_8008FFD0(int16_t node);
int16_t func_8009002C(int16_t node);
int16_t func_8008AE10(int32_t node);
int16_t func_8008AE2C(int32_t node);
void func_8008D870(int16_t node, void *tex, int32_t lod);                           /* ZOID_SetTexture */
void func_800A79B8(int32_t node, int32_t mode, uintptr_t light);                    /* ZOID_SetObjLights */
void func_800A7830(int16_t node, uintptr_t clip, uint32_t unused);
void func_800A785C(int16_t node, uintptr_t pal);
void *func_800A7884(int16_t node);
void func_8008E06C(int16_t node, uint32_t *color);                                 /* ZOID_SetColor */
void func_80092BC8(int16_t node, uint32_t *color);
void *func_8008B3A0(int16_t node);
void func_8008AE48(int32_t node, uint32_t flags);
uint32_t func_8008AE64(int16_t node);
void func_8008D6FC(int16_t node, float *pos, void *mat);
void car_mass_set(int16_t node, int16_t parent);
void entity_spawn_callback(int16_t node, int32_t children, int32_t siblings);
void model_data_load(int32_t node, int32_t mode, uint32_t mask);
int32_t track_collision(void *node, int32_t type);
int32_t track_collision_edge(void *node, int32_t type);
void func_80090770(int16_t node, uint16_t value);

/* What the files reference beyond the table. */
static void *g_math_dst, *g_math_src;
void math_utility(void *src, void *dst) { g_math_src = src, g_math_dst = dst; }
int32_t D_801170FC;
volatile uint8_t gNumModels = 1;
char *D_80118E08[3] = {"REDFLAG", "GRNFLAG", "BLUFLAG"};
static uint8_t g_palette[24];
static const char *g_pal_name;
intptr_t MBOX_FindPalette_WORK(char *name, uint16_t *out, int32_t lo, int32_t hi, int32_t err)
{
    g_pal_name = name;
    *out = 7;
    return (intptr_t)g_palette;   /* a host address, above 4 GB in this PIE */
}
char *func_800A464C(char *s, char *find) { return strstr(s, find); }
void entity_transform_apply(void *node, int32_t unlink) {}
void audio_channel_setup(void) {}

/* func_8010D85C's obj is a visual (struct pc_visual): its data is the flag, a level-object node that keeps its
 * N64 layout (0x70 bytes), and the node's state (+0x6C) is the flag's place record, here its name; both are window
 * addresses (docs/port/POINTERS.md 10). */
void func_8010D85C(struct pc_visual *obj, int16_t mode);

typedef struct {
    float rot[3][3], pos[3];
    uint8_t pad[0x98 - 0x30];
} View;

int main(void)
{
    static View views[4];
    uint8_t *w = test_rdram();
    void *wad_tex = w ? (void *)(w + 0x123450) : (void *)&views[3];   /* a WAD texture: 0x80123450 */
    uint32_t color = 0x11223344, env = 0xAABBCCDD;
    uint8_t light[40], rect[8];
    int16_t root, a, b, c;
    float pos[3] = {1.5f, -2.0f, 4.25f};
    int lod;

    memset(D_8012E700, 0xA5, sizeof(D_8012E700));   /* the N64 doesn't clear what MBOX_NewObject leaves */

    /* MBOX_NewObject: root, then two children of the root and one of the second child. */
    root = func_8008E26C(0x401, &views[0], -1, 0x42000);
    a = func_8008E26C(0x402, &views[1], root, 0);
    b = func_8008E26C(0x403, &views[2], root, 0);
    c = func_8008E26C(0x404, &views[3], b, 0);
    CHECK(root == 0 && a == 1 && b == 2 && c == 3 && D_80156990 == 4 && D_801569A8 == 4);
    CHECK(D_8015B254 == root);
    CHECK(D_8012E700[root].flags == 0x42000 && D_8012E700[root].lights == 0);
    CHECK(D_8012E700[a].obj == &views[1] && D_8012E700[a].model == 0x402);
    CHECK(D_8012E700[a].scale == 1.0f && D_8012E700[a].w == 1.0f && D_8012E700[a].mode == -1);
    CHECK(D_8012E700[a].light == NULL && D_8012E700[a].pal == NULL && D_8012E700[a].clip == NULL);
    for (lod = 0; lod < 4; lod++)
        CHECK(D_8012E700[a].tex[lod] == NULL);
    CHECK(memcmp(D_8012E700[a].prim, "\0\0\0\0", 4) == 0);
    CHECK(D_8012E700[root].child == a && D_8012E700[a].next == b && D_8012E700[b].next == -1);
    CHECK(D_8012E700[b].child == c && D_8012E700[c].child == -1);
    CHECK(func_8009002C(a) == root && func_8009002C(c) == b && func_8009002C(b) == -1);
    CHECK(func_8008FFD0(b) == a && func_8008FFD0(a) == -1);
    CHECK(func_8008E144(a) == b && func_8008AE10(a) == b && func_8008AE2C(root) == a);

    /* ZOID_SetTexture: every LOD, then one; a window address and a host pointer, whole. */
    func_8008D870(a, wad_tex, -1);
    for (lod = 0; lod < 4; lod++)
        CHECK(D_8012E700[a].tex[lod] == wad_tex);
    func_8008D870(a, &views[2], 2);
    CHECK(D_8012E700[a].tex[2] == &views[2] && D_8012E700[a].tex[1] == wad_tex && D_8012E700[a].tex[3] == wad_tex);
    CHECK(D_8012E700[b].tex[0] == NULL && D_8012E700[a].rect == NULL);

    /* The other setters and getters. */
    func_800A79B8(b, 1, (uintptr_t)light);
    CHECK(D_8012E700[b].mode == 1 && D_8012E700[b].light == light);
    func_800A7830(b, (uintptr_t)rect, 0);
    CHECK(D_8012E700[b].clip == rect);
    func_800A785C(b, (uintptr_t)g_palette);
    CHECK(D_8012E700[b].pal == g_palette);
    func_8008E06C(b, &color);
    func_80092BC8(b, &env);
    CHECK(func_800A7884(b) == D_8012E700[b].prim);
    CHECK(memcmp(D_8012E700[b].prim, &color, 4) == 0 && memcmp(D_8012E700[b].env, &env, 4) == 0);
    CHECK(func_8008B3A0(c) == &views[3]);
    func_8008AE48(c, 0x1234);
    CHECK(func_8008AE64(c) == 0x1234 && D_8012E700[c].flags == 0x1234);
    func_80090770(c, 0x777);
    CHECK(D_8012E700[c].model == 0x777);
    func_8008D6FC(c, pos, &views[0]);
    CHECK(views[3].pos[0] == 1.5f && views[3].pos[1] == -2.0f && views[3].pos[2] == 4.25f);
    CHECK(g_math_src == &views[0] && g_math_dst == &views[3]);

    /* The capture-the-flag flag: its palette lookup lands whole in the palette slot. */
    if (w) {
        uint8_t *m = w + 0x300000;              /* the flag's node, in the window */
        char *name = (char *)(w + 0x300100);
        struct pc_visual o;
        memset(m, 0, 0x70);
        memset(&o, 0, sizeof(o));
        *(int16_t *)(m + 0xE) = c;
        strcpy(name, "FLAG_GRN_OFF3");
        *(uint32_t *)(m + 0x6C) = (uint32_t)(uintptr_t)name;
        o.data = (uint32_t)(uintptr_t)m;
        D_8012E700[c].pal = NULL;
        func_8010D85C(&o, 1);
        CHECK(g_pal_name == D_80118E08[1]);
        CHECK(D_8012E700[c].pal == g_palette && (m[4] & 1) && *(int16_t *)(m + 0x5A) == 10 && o.index == 3);
        CHECK(o.func == (void (*)(void *, short))audio_channel_setup && o.timeStamp == 0.0f);
    }

    /* Hiding walks the children; the collision walk follows them. */
    D_8012E700[root].flags = D_8012E700[a].flags = D_8012E700[b].flags = D_8012E700[c].flags = 0;
    model_data_load(root, 2, 1);
    CHECK(D_8012E700[root].flags == 0x100 && D_8012E700[a].flags == 0x100 && D_8012E700[b].flags == 0x100);
    CHECK(D_8012E700[c].flags == 0x100);
    CHECK(track_collision(&D_8012E700[root], 0) == 0);
    D_8012E700[c].flags = 0;
    CHECK(track_collision(&D_8012E700[root], 0) == 1 && track_collision_edge(&D_8012E700[b], 0) == 1);
    D_8012E700[c].flags = 0x100;
    CHECK(track_collision_edge(&D_8012E700[b], 0) == 0);

    /* Re-parent c under a, then remove a's subtree. */
    car_mass_set(c, a);
    CHECK(D_8012E700[b].child == -1 && D_8012E700[a].child == c && D_8012E700[c].next == -1);
    entity_spawn_callback(a, 1, 0);
    CHECK(D_8012E700[a].model == 0xFFFF && D_8012E700[c].model == 0xFFFF && D_8012E700[root].child == b);
    entity_spawn_callback(b, 1, 0);
    CHECK(D_80156990 == 1 && D_8012E700[root].child == -1);
    /* A free slot is reused. */
    CHECK(func_8008E26C(0x405, &views[1], root, 0) == 1 && D_80156990 == 2);

    return TEST_DONE();
}

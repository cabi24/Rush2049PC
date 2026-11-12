/* port/assets/tests/test_zoid.c: the game's own scene-node (ObjInstanceList, ObjInstanceList) functions on the node's
 * PC layout (docs/port/POINTERS.md 9): MBOX_NewObject and its linking, the ZOID setters and getters, the
 * capture-the-flag palette lookup, unlinking, hiding and the collision walk. The table is defined here as the model
 * draw (particle_knot) reads it; each game file has its own view of it, which must put every field where that
 * view does. Pointers (game data, a WAD's texture, overlay A's ComponentLights, a stack rectangle) must arrive whole.
 * game: src/blob/ZOID_AddObject.c src/blob/ZOID_InsertObject.c src/blob/ZOID_LastSibling.c
 * game: src/blob/ZOID_ObjPrevObj.c src/blob/ZOID_ObjParent.c src/blob/MBOX_GetNext.c src/blob/MBOX_GetChild.c
 * game: src/blob/ZOID_SetTexture.c src/blob/ZOID_SetObjLights.c src/blob/zoid_set_obj_tile_size.c src/blob/zoid_set_obj_palette.c
 * game: src/blob/ZOID_GetColor.c src/blob/ZOID_SetColor.c src/blob/ZOID_SetEnvColor.c src/blob/ZOID_FindMatrix.c
 * game: src/blob/ZOID_SetObjectFlags.c src/blob/ZOID_ObjectFlags.c src/blob/ZOID_UpdateObject.c src/blob/AnimateFlag.c
 * game: src/blob/ZOID_SetObjectParent.c src/blob/RemoveObjectTree.c src/blob/MBOX_HideObject.c
 * game: src/blob/zoid_subtree_visible.c src/blob/zoid_node_visible.c src/blob/ZOID_SetObjectDef.c
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

Node ObjInstanceList[600];
int32_t gNumObjects, gMaxObjects;   /* nodes in use, high-water mark */
int16_t ObjHead = -1;          /* the root list */

int16_t ZOID_AddObject(int32_t id, void *owner, int16_t parent, int32_t flags);   /* MBOX_NewObject */
int16_t ZOID_LastSibling(int16_t node);
int16_t ZOID_ObjPrevObj(int16_t node);
int16_t ZOID_ObjParent(int16_t node);
int16_t MBOX_GetNext(int32_t node);
int16_t MBOX_GetChild(int32_t node);
void ZOID_SetTexture(int16_t node, void *tex, int32_t lod);
void ZOID_SetObjLights(int32_t node, int32_t mode, uintptr_t light);
void zoid_set_obj_tile_size(int16_t node, uintptr_t clip, uint32_t unused);
void zoid_set_obj_palette(int16_t node, uintptr_t pal);
void *ZOID_GetColor(int16_t node);
void ZOID_SetColor(int16_t node, uint32_t *color);
void ZOID_SetEnvColor(int16_t node, uint32_t *color);
void *ZOID_FindMatrix(int16_t node);
void ZOID_SetObjectFlags(int32_t node, uint32_t flags);
uint32_t ZOID_ObjectFlags(int16_t node);
void ZOID_UpdateObject(int16_t node, float *pos, void *mat);
void ZOID_SetObjectParent(int16_t node, int16_t parent);
void RemoveObjectTree(int16_t node, int32_t children, int32_t siblings);
void MBOX_HideObject(int32_t node, int32_t mode, uint32_t mask);
int32_t zoid_subtree_visible(void *node, int32_t type);
int32_t zoid_node_visible(void *node, int32_t type);
void ZOID_SetObjectDef(int16_t node, uint16_t value);

/* What the files reference beyond the table. */
static void *g_math_dst, *g_math_src;
void CopyUV(void *src, void *dst) { g_math_src = src, g_math_dst = dst; }
int32_t gPauseState;
volatile uint8_t gNumModels = 1;
char *flag_pals[3] = {"REDFLAG", "GRNFLAG", "BLUFLAG"};
static uint8_t g_palette[24];
static const char *g_pal_name;
intptr_t MBOX_FindPalette_WORK(char *name, uint16_t *out, int32_t lo, int32_t hi, int32_t err)
{
    g_pal_name = name;
    *out = 7;
    return (intptr_t)g_palette;   /* a host address, above 4 GB in this PIE */
}
char *game_strstr(char *s, char *find) { return strstr(s, find); }
void ReleaseEnviron(void *node, int32_t unlink) {}
void AnimateObjSwapGeneric(void) {}

/* AnimateFlag's obj is a visual (struct pc_visual): its data is the flag, a level-object node (struct tTarget, the
 * host layout), and the node's data is the flag's place record, here its name; both are pointers
 * (docs/port/POINTERS.md 10.2). */
void AnimateFlag(struct pc_visual *obj, int16_t mode);

typedef struct {
    float rot[3][3], pos[3];
    uint8_t pad[0x98 - 0x30];
} View;

int main(void)
{
    static View views[4];
    static uint8_t wad[0x200], world[0x20];   /* a WAD's texture record, a track WAD's world record */
    void *wad_tex = wad + 0x150;
    uint32_t color = 0x11223344, env = 0xAABBCCDD;
    uint8_t light[40], rect[8];
    int16_t root, a, b, c;
    float pos[3] = {1.5f, -2.0f, 4.25f};
    int lod;

    memset(ObjInstanceList, 0xA5, sizeof(ObjInstanceList));   /* the N64 doesn't clear what MBOX_NewObject leaves */

    /* MBOX_NewObject: root, then two children of the root and one of the second child. */
    root = ZOID_AddObject(0x401, &views[0], -1, 0x42000);
    a = ZOID_AddObject(0x402, &views[1], root, 0);
    b = ZOID_AddObject(0x403, &views[2], root, 0);
    c = ZOID_AddObject(0x404, &views[3], b, 0);
    CHECK(root == 0 && a == 1 && b == 2 && c == 3 && gNumObjects == 4 && gMaxObjects == 4);
    CHECK(ObjHead == root);
    CHECK(ObjInstanceList[root].flags == 0x42000 && ObjInstanceList[root].lights == 0);
    CHECK(ObjInstanceList[a].obj == &views[1] && ObjInstanceList[a].model == 0x402);
    CHECK(ObjInstanceList[a].scale == 1.0f && ObjInstanceList[a].w == 1.0f && ObjInstanceList[a].mode == -1);
    CHECK(ObjInstanceList[a].light == NULL && ObjInstanceList[a].pal == NULL && ObjInstanceList[a].clip == NULL);
    for (lod = 0; lod < 4; lod++)
        CHECK(ObjInstanceList[a].tex[lod] == NULL);
    CHECK(memcmp(ObjInstanceList[a].prim, "\0\0\0\0", 4) == 0);
    CHECK(ObjInstanceList[root].child == a && ObjInstanceList[a].next == b && ObjInstanceList[b].next == -1);
    CHECK(ObjInstanceList[b].child == c && ObjInstanceList[c].child == -1);
    CHECK(ZOID_ObjParent(a) == root && ZOID_ObjParent(c) == b && ZOID_ObjParent(b) == -1);
    CHECK(ZOID_ObjPrevObj(b) == a && ZOID_ObjPrevObj(a) == -1);
    CHECK(ZOID_LastSibling(a) == b && MBOX_GetNext(a) == b && MBOX_GetChild(root) == a);

    /* ZOID_SetTexture: every LOD, then one; each pointer whole. */
    ZOID_SetTexture(a, wad_tex, -1);
    for (lod = 0; lod < 4; lod++)
        CHECK(ObjInstanceList[a].tex[lod] == wad_tex);
    ZOID_SetTexture(a, &views[2], 2);
    CHECK(ObjInstanceList[a].tex[2] == &views[2] && ObjInstanceList[a].tex[1] == wad_tex && ObjInstanceList[a].tex[3] == wad_tex);
    CHECK(ObjInstanceList[b].tex[0] == NULL && ObjInstanceList[a].rect == NULL);

    /* The other setters and getters. */
    ZOID_SetObjLights(b, 1, (uintptr_t)light);
    CHECK(ObjInstanceList[b].mode == 1 && ObjInstanceList[b].light == light);
    zoid_set_obj_tile_size(b, (uintptr_t)rect, 0);
    CHECK(ObjInstanceList[b].clip == rect);
    zoid_set_obj_palette(b, (uintptr_t)g_palette);
    CHECK(ObjInstanceList[b].pal == g_palette);
    ZOID_SetColor(b, &color);
    ZOID_SetEnvColor(b, &env);
    CHECK(ZOID_GetColor(b) == ObjInstanceList[b].prim);
    CHECK(memcmp(ObjInstanceList[b].prim, &color, 4) == 0 && memcmp(ObjInstanceList[b].env, &env, 4) == 0);
    CHECK(ZOID_FindMatrix(c) == &views[3]);
    ZOID_SetObjectFlags(c, 0x1234);
    CHECK(ZOID_ObjectFlags(c) == 0x1234 && ObjInstanceList[c].flags == 0x1234);
    ZOID_SetObjectDef(c, 0x777);
    CHECK(ObjInstanceList[c].model == 0x777);
    ZOID_UpdateObject(c, pos, &views[0]);
    CHECK(views[3].pos[0] == 1.5f && views[3].pos[1] == -2.0f && views[3].pos[2] == 4.25f);
    CHECK(g_math_src == &views[0] && g_math_dst == &views[3]);

    /* The capture-the-flag flag: its palette lookup lands whole in the palette slot. */
    {
        static struct tTarget flag;             /* the flag's node */
        struct tTarget *m = &flag;
        char *name = (char *)world;             /* its world object (a track WAD record) */
        struct pc_visual o;
        memset(m, 0, sizeof *m);
        memset(&o, 0, sizeof(o));
        m->dlIdx = c;                            /* its handle: the low half is the model id */
        strcpy(name, "FLAG_GRN_OFF3");
        m->data = name;
        o.data = (uintptr_t)m;
        ObjInstanceList[c].pal = NULL;
        AnimateFlag(&o, 1);
        CHECK(g_pal_name == flag_pals[1]);
        CHECK(ObjInstanceList[c].pal == g_palette && (m->Flags & 1) && m->frames == 10 && o.index == 3);
        CHECK(o.func == (void (*)(void *, short))AnimateObjSwapGeneric && o.timeStamp == 0.0f);
    }

    /* Hiding walks the children; the collision walk follows them. */
    ObjInstanceList[root].flags = ObjInstanceList[a].flags = ObjInstanceList[b].flags = ObjInstanceList[c].flags = 0;
    MBOX_HideObject(root, 2, 1);
    CHECK(ObjInstanceList[root].flags == 0x100 && ObjInstanceList[a].flags == 0x100 && ObjInstanceList[b].flags == 0x100);
    CHECK(ObjInstanceList[c].flags == 0x100);
    CHECK(zoid_subtree_visible(&ObjInstanceList[root], 0) == 0);
    ObjInstanceList[c].flags = 0;
    CHECK(zoid_subtree_visible(&ObjInstanceList[root], 0) == 1 && zoid_node_visible(&ObjInstanceList[b], 0) == 1);
    ObjInstanceList[c].flags = 0x100;
    CHECK(zoid_node_visible(&ObjInstanceList[b], 0) == 0);

    /* Re-parent c under a, then remove a's subtree. */
    ZOID_SetObjectParent(c, a);
    CHECK(ObjInstanceList[b].child == -1 && ObjInstanceList[a].child == c && ObjInstanceList[c].next == -1);
    RemoveObjectTree(a, 1, 0);
    CHECK(ObjInstanceList[a].model == 0xFFFF && ObjInstanceList[c].model == 0xFFFF && ObjInstanceList[root].child == b);
    RemoveObjectTree(b, 1, 0);
    CHECK(gNumObjects == 1 && ObjInstanceList[root].child == -1);
    /* A free slot is reused. */
    CHECK(ZOID_AddObject(0x405, &views[1], root, 0) == 1 && gNumObjects == 2);

    return TEST_DONE();
}

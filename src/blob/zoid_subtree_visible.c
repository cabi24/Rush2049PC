#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct TrackNode { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 left; s16 right; s16 mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } TrackNode;
VIEW(TrackNode[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
s32 zoid_subtree_visible(TrackNode *node, s32 type)
{
  u32 mask = 256 << type;
  while (1) {
    if (!(node->flags & mask)) return 1;
    if (node->left >= 0 && zoid_subtree_visible(&ObjInstanceList[node->left], type)) return 1;
    if (node->right >= 0) { node = &ObjInstanceList[node->right]; continue; }
    return 0;
  }
}

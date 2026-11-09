/* zoid_subtree_visible.c: whether anything in a part of the scene tree shows in a view (the N64's; part of its
 * scene walk). */
#include "game/game.h"

/* Whether `node`, one of its later siblings, or anything below them is not hidden in view `view` (OBJ_HIDDEN_VIEW0
 * << view). */
s32 zoid_subtree_visible(ObjectInstance *node, s32 view)
{
  u32 mask = OBJ_HIDDEN_VIEW0 << view;
  while (1) {
    if (!(node->flags & mask)) return 1;
    if (node->child >= 0 && zoid_subtree_visible(&ObjInstanceList[node->child], view)) return 1;
    if (node->next >= 0) { node = &ObjInstanceList[node->next]; continue; }
    return 0;
  }
}

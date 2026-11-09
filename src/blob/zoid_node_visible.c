/* zoid_node_visible.c: whether the scene draw must visit a node in a view (the N64's; part of its scene walk). */
#include "game/game.h"

/* Whether node `node` is to be visited in view `view`: yes unless it is hidden in that view (OBJ_HIDDEN_VIEW0 << view),
 * and even then yes if anything below it is not (zoid_subtree_visible on its children), since hiding a node in a
 * view does not hide its children. */
s32 zoid_node_visible(void *node, s32 view)
{
  ObjectInstance *obj = (ObjectInstance *) node;
  if (!(obj->flags & (OBJ_HIDDEN_VIEW0 << view)))
  {
    return 1;
  }
  if ((obj->child >= 0) && (zoid_subtree_visible(ObjInstanceList + obj->child, view) != 0))
  {
    return 1;
  }
  return 0;
}

/* ZOID_LastSibling.c: the last node of a sibling list (MAT3 zoid.c, ZOID_LastSibling()). */
#include "game/game.h"

/* The last node of the sibling list `node` is in, following next links from it; a list that loops back to `node`
 * stops there. */
s16 ZOID_LastSibling(s16 node)
{
  s16 last;
  s16 i = node;
  do
  {
    last = i;
    i = (ObjInstanceList + i)->next;
  } while ((i >= 0) && (i != node));
  return last;
}

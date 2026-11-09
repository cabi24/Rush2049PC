/* InterruptFxTag: an fx tag whose voice was taken goes back to wait (MAT3 audiomgr.c's _InterruptFxTag). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Marks the tag interrupted and moves it from its list to the head of gInterruptedFxTags, where the priority
 * pass ranks it again (a looped sound restarts when it ranks high enough). */
void InterruptFxTag(tFxSequence *tag)
{
    tag->state = kFxInterrupted;
    RemoveLLE(tag->list, tag);
    InsertLLE(&gInterruptedFxTags, tag, gInterruptedFxTags.head);
    tag->list = &gInterruptedFxTags;
}

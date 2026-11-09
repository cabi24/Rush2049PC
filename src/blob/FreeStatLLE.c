#include "game/game.h"

typedef struct Node {struct Node *prev,*next;} Node;
/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). A list
 * flagged n64 links its elements by 32-bit window addresses (the level-object nodes, POINTERS.md 10). */
typedef struct Pool {u8 doubly,n64;s32 count,size;void *mem;Node *active,*free;} Pool;
#define PREV(n) ((Node *)pc_statll_link(pool->n64,n,0))
#define NEXT(n) ((Node *)pc_statll_link(pool->n64,n,1))
void FreeStatLLE(Pool *pool,Node *node) {
 Node *cur,*prev;
 if(node) {
  if(pool->doubly) {
   if(PREV(node))pc_statll_set_link(pool->n64,PREV(node),1,NEXT(node));
   cur=NEXT(node);
   if(cur)pc_statll_set_link(pool->n64,cur,0,PREV(node));
   else pool->active=PREV(node);
  }else{
   cur=pool->active;prev=0;
   while(cur) {
    if(node==cur) {
     if(prev)pc_statll_set_link(pool->n64,prev,0,PREV(node));
     else pool->active=PREV(node);
     break;
    }
    prev=cur;cur=PREV(cur);
   }
  }
  pc_statll_set_link(pool->n64,node,0,pool->free);pool->free=node;
 }
}

#include "game/game.h"

typedef struct { u32 first,second; } Pair;
VIEW(Pair, gJust);
#define gJust gJust_view
void TB_SetJust(u32 first,u32 second) {Pair *p=&gJust;p->first=first;p->second=second;}

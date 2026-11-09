#include "game/game.h"

typedef struct Header {u16 tag;u16 flags;} Header;
void ZOID_ClearPolyFlags(Header *header,u16 mask) {header->flags &= ~mask;}

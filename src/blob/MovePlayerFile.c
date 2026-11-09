#include "game/game.h"

typedef struct PlayerFileNode {
    struct PlayerFileNode *next;
} PlayerFileNode;

VIEW(s32, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view

VIEW(s32, gGhostFiles);
#define gGhostFiles gGhostFiles_view

void MovePlayerFile(intptr_t arg0, intptr_t arg1) {
    /* the profile and ghost file lists (playerscr.c's struct PlayerFiles, carscr.c's RecordList): their heads */
    struct { u32 unknown0, unknown4; void *head, *tail; } *player_files = (void *) &gPlayerFiles;
    struct { u8 indirect, doubly, unknown02[2]; s32 count; void *first, *last; } *ghost_files = (void *) &gGhostFiles;
    PlayerFileNode *temp_v1;
    PlayerFileNode *var_v0;
    intptr_t var_v0_2;
    void *temp_v1_2;

    var_v0 = player_files->head;
    if (var_v0 != NULL) {
loop_1:
        temp_v1 = var_v0->next;
        if (arg0 == M2C_FIELD(temp_v1, intptr_t *, PROFILE_OFS(8))) {
            M2C_FIELD(temp_v1, intptr_t *, PROFILE_OFS(8)) = arg1;
            return;
        }
        var_v0 = temp_v1->next;
        if (var_v0 == NULL) {
            goto block_4;
        }
        goto loop_1;
    }
block_4:
    var_v0_2 = (intptr_t) ghost_files->first;
    if (var_v0_2 != 0) {
loop_5:
        temp_v1_2 = M2C_FIELD(var_v0_2, void **, 0);
        if (arg0 == M2C_FIELD(temp_v1_2, intptr_t *, GHOST_OFS(4))) {
            M2C_FIELD(temp_v1_2, intptr_t *, GHOST_OFS(4)) = arg1;
            return;
        }
        var_v0_2 = M2C_FIELD(temp_v1_2, intptr_t *, 0);
        if (var_v0_2 == 0) {

        } else {
            goto loop_5;
        }
    }
}

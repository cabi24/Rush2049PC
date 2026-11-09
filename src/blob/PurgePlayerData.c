#include "game/game.h"

void PurgePlayerData(void **arg0) {
    void *temp_v0;
    void **temp_a0;

    temp_v0 = *arg0;
    if (M2C_FIELD(temp_v0, intptr_t *, PROFILE_OFS(0x2C)) != 0) {
        temp_a0 = M2C_FIELD(temp_v0, void ***, PROFILE_OFS(8));
        if (temp_a0 != NULL) {
            PS_PurgeFile(temp_a0);
            M2C_FIELD(temp_v0, intptr_t *, PROFILE_OFS(0x2C)) = 0;
        }
    }
}


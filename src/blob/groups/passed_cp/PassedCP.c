#include "game/game.h"

/* Accessed 0x7EC prefix of the authentic 0x808 model record. This function
 * receives one pointer; it never uses sizeof(Model) or model-array indexing. */
typedef struct Model {
    u8 pad000[0x7C6];
    s16 slot;
    u8 pad7C8[0x7D4 - 0x7C8];
    u32 flags;
    u8 pad7D8[0x7DC - 0x7D8];
    s8 lap_enabled;
    u8 pad7DD[0x7E2 - 0x7DD];
    s16 last_cp;
    s16 next_cp;
    u8 pad7E6[2];
    s8 laps;
    u8 pad7E9;
    s8 finish_a;
    s8 finish_b;
} Model;

typedef struct GameCar {
    u8 pad000[0xEE];
    s8 place;
    s8 place_locked;
    u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21];
    s8 finish_a;
    s8 finish_b;
    u8 pad30A[2];
    f32 finish_timer;
    u8 pad310;
    s8 flags311;
    u8 pad312[0x380 - 0x312]; void *input; u8 pad312_384[0x3B8 - 0x384];
} GameCar;

/* Accessed ten-byte prefix of the checkpoint header; no array indexing. */
typedef struct Track {
    s16 unknown0;
    s16 loop_start;
    s16 finish_line;
    s16 before_finish;
    s16 count;
} Track;

VIEW(Track, gCheckpoints);
#define gCheckpoints gCheckpoints_view

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view

void PassedCP(Model *m, f32 time)
{
    s32 old_remaining;
    s32 player;
    PC_LINK *link;
    GameCar *gc;
    s32 completed_lap;

    completed_lap = 0;
    player = m->slot;
    m->last_cp = m->next_cp;
    m->next_cp = GetNextCheckpoint(m->last_cp);
    if (m->last_cp == gCheckpoints.finish_line && m->lap_enabled &&
        gRace != 1 && !(gGameState & 8)) {
        m->laps++;
        completed_lap = 1;
    }
    if (!m->lap_enabled && m->last_cp == gCheckpoints.before_finish) {
        m->lap_enabled = 1;
    }
    link = &PC_LINKS[player];
    gc = PC_CAR(player);
    if (PC_LINKP_OWNER(link) == 6) {
        if (m->last_cp == gCheckpoints.finish_line && !(gGameState & 8)) {
            ShowCheckpointTime(player, time);
        }
        gOneMoreCP = m->next_cp == gCheckpoints.finish_line &&
            m->laps + 1 == gNumLaps;
    }
    if (completed_lap) {
        RecordLap(m->slot, time);
        if (m->laps == gNumLaps && !gc->place_locked) {
            CarReportsGameOver(m->slot, time);
        }
        if (PC_LINKP_OWNER(link) == 6 && (gRace != 2 || player == 0)) {
            old_remaining = gNumLapsLeft;
            gNumLapsLeft = gNumLapsLeft < gNumLaps - m->laps ?
                gNumLapsLeft : gNumLaps - m->laps;
            if (m->laps == gNumLaps && gc->place_locked != -1) {
                PC_FINISH_CAM(player) = 1;
                PC_FINISH_CAM_TIME(player) = gSeconds;
                switch (gc->place) {
                case 0:
                    PlayGameSound(64, player, 1, 1);
                    break;
                case 1:
                    PlayGameSound(68, player, 1, 1);
                    break;
                case 2:
                    PlayGameSound(67, player, 1, 1);
                    break;
                default:
                    PlayGameSound(65, player, 1, 1);
                    break;
                }
                gc->finish_a = 1;
                gc->finish_b = 1;
                gc->flags311 |= 1;
                gc->finish_timer = 0.0f;
                m->finish_a = 1;
                m->finish_b = 1;
                m->flags &= ~8;
            } else if (old_remaining != gNumLapsLeft) {
                if (gNumLapsLeft == 1) {
                    PlayGameSound(74, player, 1, 1);
                } else {
                    AudPlaySequence(0, 2.0f, 1, 0, 2, gNumLapsLeft + 79, 80);
                }
            }
        }
    }
}


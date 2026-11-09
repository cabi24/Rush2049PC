/* PassedCP: a car passes a checkpoint: laps, the finish, the lap calls (MAT3 checkpoint.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the fields this reads and sets. */
typedef struct Model {
    u8 pad000[0x7C6];
    s16 slot;          /* +0x7C6 the car slot */
    u8 pad7C8[0x7D4 - 0x7C8];
    u32 appearance;    /* +0x7D4 appearance flags; 8 a ghost (not collidable) */
    u8 pad7D8[0x7DC - 0x7D8];
    s8 started;        /* +0x7DC it has passed the start checkpoint, so laps count */
    u8 pad7DD[0x7E2 - 0x7DD];
    s16 last_cp_index; /* +0x7E2 */
    s16 next_cp_index; /* +0x7E4 */
    u8 pad7E6[2];
    s8 laps;           /* +0x7E8 laps completed */
    u8 pad7E9;
    s8 collidable;     /* +0x7EA */
    s8 solid;          /* +0x7EB */
} Model;

/* The car record (MAT3 tCarData; the PC's pc_car, host layout; MAT3's offsets past the visuals are 0x18 more). */
typedef struct GameCar {
    u8 pad000[0xEE];
    s8 place;          /* +0x0EE its place, 0 the first */
    s8 place_locked;   /* +0x0EF 1 once it finished, -1 out of the race */
    u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21];
    s8 collidable;     /* +0x308 */
    s8 solid;          /* +0x309 */
    u8 pad30A[2];
    f32 collide_time;  /* +0x30C */
    u8 pad310;
    s8 collide_state;  /* +0x311 */
    u8 pad312[0x380 - 0x312]; void *input; u8 pad312_384[0x3B8 - 0x384];
} GameCar;

/* Model `m` has crossed its next checkpoint at race time `time`. It becomes the car's last checkpoint; crossing
 * the finish line once started completes a lap (not in practice, nor in the attract mode's demo races). For a car
 * this machine drives (link owner 6): the finish line shows the checkpoint time, gOneMoreCP says the next
 * checkpoint is the finish of the last lap, and a completed lap (only the first player's in a ghost race) brings the
 * laps-left count down with its call (sound 74 for the last lap, else a sequence: the count's sound then 80), or
 * on the last lap the finish: the finish camera, the call for its place, and the car made solid again. A car that completes the last lap and has no place
 * yet reports its finish (CarReportsGameOver). */
void PassedCP(Model *m, f32 time)
{
    s32 prevLapsLeft;
    s32 slot;
    PC_LINK *link;
    GameCar *gc;
    s32 finish;

    finish = 0;
    slot = m->slot;
    m->last_cp_index = m->next_cp_index;
    m->next_cp_index = GetNextCheckpoint(m->last_cp_index);
    if (m->last_cp_index == gCheckpoints.finishPt && m->started &&
        gRace != kPractice && !(gGameState & GS_DEMO)) {
        m->laps++;
        finish = 1;
    }
    if (!m->started && m->last_cp_index == gCheckpoints.startPt) {
        m->started = 1;
    }
    link = &PC_LINKS[slot];
    gc = PC_CAR(slot);
    if (PC_LINKP_OWNER(link) == 6) {
        if (m->last_cp_index == gCheckpoints.finishPt && !(gGameState & GS_DEMO)) {
            ShowCheckpointTime(slot, time);
        }
        gOneMoreCP = m->next_cp_index == gCheckpoints.finishPt &&
            m->laps + 1 == gNumLaps;
    }
    if (finish) {
        RecordLap(m->slot, time);
        if (m->laps == gNumLaps && !gc->place_locked) {
            CarReportsGameOver(m->slot, time);
        }
        if (PC_LINKP_OWNER(link) == 6 && (gRace != kGhost || slot == 0)) {
            prevLapsLeft = gNumLapsLeft;
            gNumLapsLeft = gNumLapsLeft < gNumLaps - m->laps ?
                gNumLapsLeft : gNumLaps - m->laps;
            if (m->laps == gNumLaps && gc->place_locked != -1) {
                PC_FINISH_CAM(slot) = 1;
                PC_FINISH_CAM_TIME(slot) = gSeconds;
                switch (gc->place) {
                case 0:
                    PlayGameSound(64, slot, 1, 1);
                    break;
                case 1:
                    PlayGameSound(68, slot, 1, 1);
                    break;
                case 2:
                    PlayGameSound(67, slot, 1, 1);
                    break;
                default:
                    PlayGameSound(65, slot, 1, 1);
                    break;
                }
                gc->collidable = 1;
                gc->solid = 1;
                gc->collide_state |= 1;
                gc->collide_time = 0.0f;
                m->collidable = 1;
                m->solid = 1;
                m->appearance &= ~8;
            } else if (prevLapsLeft != gNumLapsLeft) {
                if (gNumLapsLeft == 1) {
                    PlayGameSound(74, slot, 1, 1);
                } else {
                    AudPlaySequence(0, 2.0f, 1, 0, 2, gNumLapsLeft + 79, 80);
                }
            }
        }
    }
}

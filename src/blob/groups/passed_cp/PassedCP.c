/* PassedCP: a car passes a checkpoint: laps, the finish, the lap calls (MAT3 checkpoint.c). */
#include "game/game.h"

/* Model `m` has crossed its next checkpoint at race time `time`. It becomes the car's last checkpoint; crossing
 * the finish line once started completes a lap (not in practice, nor in the attract mode's demo races). For a car
 * this machine drives (link owner 6): the finish line shows the checkpoint time, gOneMoreCP says the next
 * checkpoint is the finish of the last lap, and a completed lap (only the first player's in a ghost race) brings the
 * laps-left count down with its call (sound 74 for the last lap, else a sequence: the count's sound then 80), or
 * on the last lap the finish: the finish camera, the call for its place, and the car made solid again. A car that completes the last lap and has no place
 * yet reports its finish (CarReportsGameOver). */
void PassedCP(tModelData *m, f32 time)
{
    s32 prevLapsLeft;
    s32 slot;
    PC_LINK *link;
    tCarData *gc;
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
    gc = &pc_cars[slot];
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

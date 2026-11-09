#include "game/game.h"

/* The drones' catch-up (MAT3 drones.c, model.c's SetCatchup): each frame every drone this machine drives gets a
 * speed scale (drone_scale) and a boost (time_boost) by how it stands against the humans. The code keeps the arcade
 * game's style (its types, game_car, FoundCoin); RecordGhost and RecordPractice are folded in. */
float fabsf(float);

typedef signed char S8;
typedef unsigned char U8;
typedef signed short S16;
typedef unsigned short U16;
typedef signed int S32;

typedef float F32;

/* DoDrones' locals are indexed by a car slot or count the cars: on the PC as many as a race may hold
 * (pc_game_types.h, docs/port/DECOUPLE.md 8). MAX_LINKS has no other use here. */
#define MAX_LINKS PC_MAX_LINKS
#define DRONE 1
#define HUMAN 2

/* The model record (MAT3 tModelData): the fields the catch-up reads and sets. */
typedef struct MODELDAT {
    /* 0x000 */ U8 pad0[0x400];
    /* 0x400 */ F32 catchup;
    /* 0x404 */ U8 pad404[0x7C6 - 0x404];
    /* 0x7C6 */ S16 slot;
    /* 0x7C8 */ S16 in_game;
    /* 0x7CA */ S16 we_control;
    /* 0x7CC */ S8 drone_type;
    /* 0x7CD */ U8 pad7CD[0x7E6 - 0x7CD];
    /* 0x7E6 */ S16 drone_target;
    /* 0x7E8 */ U8 pad7E8[4];
    /* 0x7EC */ F32 drone_scale;
    /* 0x7F0 */ F32 time_boost;
    /* 0x7F4 */ U8 pad7F4[0x808 - 0x7F4];
} MODELDAT;

typedef struct {
    /* 0x000 */ U8 pad0[8];
    /* 0x008 */ F32 dr_pos[3];
    /* 0x014 */ U8 pad14[0xEE - 0x14];
    /* 0x0EE */ S8 place;
    /* 0x0EF */ U8 pad0EF[0x100 - 0xEF];
    /* 0x100 */ F32 distance;
    /* 0x104 */ U8 pad104[0x110 - 0x104]; struct pc_visual visuals[21]; U8 pad104_308[0x356 - 0x308];
    /* 0x356 */ S16 weight_index;
    /* 0x358 */ U8 pad358;
    /* 0x359 */ S8 state;
    /* 0x35A */ U8 pad35A[0x380 - 0x35A]; void *input; U8 pad35A_384[0x3B8 - 0x384];
} CAR_DATA;

/* The arcade's names for the game's data. */
#define game_car gGameCar

#define num_active_cars gNumActiveCars


#define coast_flag gCoastFlag
#define end_game_flag gEndGame
#define lap_flag gOneMoreCP

#define FoundCoin LinearInterp

/* Maps `input` from the range in_bound1..in_bound2 to out_bound1..out_bound2, clamped to its ends (the arcade's
 * FoundCoin); the middle of the outputs when the range is empty. */
F32 LinearInterp(F32 in_bound1, F32 in_bound2, F32 input, F32 out_bound1, F32 out_bound2)
{
    F32 slope, offset;

    if (in_bound1 < in_bound2) {
        if (input < in_bound1)
            return (out_bound1);
        else if (input > in_bound2)
            return (out_bound2);
    } else {
        if (input < in_bound2)
            return (out_bound2);
        else if (input > in_bound1)
            return (out_bound1);
    }

    if (fabsf(in_bound1 - in_bound2) < 1e-3f)
        return ((out_bound1 + out_bound2) * .5f);

    slope = (out_bound1 - out_bound2) / (in_bound1 - in_bound2);
    offset = out_bound1 - slope * in_bound1;

    return (slope * input + offset);
}



/* Sets every drone's catch-up for this frame. The cars are ranked by place; in the demo every drone runs at scale 1.
 * In the first 5 seconds the drones get a small boost by their place. Then each human not in a pack with another
 * gets the nearest drone just ahead of or behind them as its rival (drone_target); a drone with a rival speeds up
 * (time_boost) when behind it and brakes (drone_scale) when ahead, by distance; the leading drone without one keeps
 * near the second car, the last near the one before it, a middle one between its neighbours or with the side that
 * has more humans. How hard is set by the difficulty, the drone's place and, in a circuit, the drone's skill on the
 * track (gCircuitCars). No boost once coasting or ended; no push past the last human on the last lap. Each value
 * moves to its new one by at most 0.01 a frame (PC_FSCALE: per 30 Hz frame). */
void DoDrones(void)
{
    S16 i, j, k, indx, index, index2, index3, temp, high_index;
    S16 high_place;
    S16 total_drones[MAX_LINKS], human_slots[MAX_LINKS], drones[MAX_LINKS];
    S16 car_in_conflict[MAX_LINKS], cars_in_order[MAX_LINKS], my_place;
    S16 ttype, place[MAX_LINKS], human_count, drone_count;
    F32 scale, delta_dist, delta_dist2, slope, offset, delta[3];
    F32 time_scale, target_dist, car_distance[MAX_LINKS][MAX_LINKS];
    F32 max_brake, min_brake, max_boost, min_boost, temp1;
    F32 place_scale, diff_scale;
    MODELDAT *m;
    CAR_DATA *gc;

    SetCatchup();

    for (i = 0; i < num_active_cars; i++) {
        high_place = PC_MODEL_SLOT(i);
        place[high_place] = PC_CAR_PLACE(high_place);
    }

    human_count = drone_count = 0;

    for (i = 0; i < num_active_cars; i++) {
        index = 0;
        for (j = 0; j < num_active_cars; j++) {
            index = PC_MODEL_SLOT(j);

            if (place[index] == i)
                break;
        }

        cars_in_order[i] = index;

        if (PC_MODEL_DRONE_TYPE(index) == HUMAN)
            human_slots[human_count++] = index;
        else
            drones[drone_count++] = index;
    }

    for (i = 0, j = 0; i < drone_count; i++)
        PC_CAR_WEIGHT_INDEX(drones[i]) = i;

    if (gGameState & GS_DEMO) {
        for (i = 0; i < num_active_cars; i++) {
            index = PC_MODEL_SLOT(i);

            if (PC_MODEL_DRONE_TYPE(index) == DRONE)
                PC_MODEL_DRONE_SCALE(index) = 1;
        }

        return;
    }

    if (drone_count) {
        if (gModelTime < 5) {
            for (i = 0; i < drone_count; i++) {
                j = drone_count - i - 1;
                PC_MODEL_TIME_BOOST(drones[i]) = ((F32)(j) * .02f) + .98f;
                PC_MODEL_DRONE_SCALE(drones[i]) = 1;
            }
        } else {
            for (i = 0; i < num_active_cars; i++) {
                index = PC_MODEL_SLOT(i);
                car_in_conflict[index] = 0;
                gc = PC_CAR(index);

                for (j = i; j < num_active_cars; j++) {
                    index2 = PC_MODEL_SLOT(j);
                    if (index == index2)
                        car_distance[index][index2] = 99999999;
                    else {
                        for (k = 0; k < 3; k++)
                            delta[k] = gc->dr_pos[k] - PC_CAR_POS(index2)[k];

                        delta_dist = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];

                        car_distance[index][index2] =
                            car_distance[index2][index] = delta_dist;
                    }
                }
            }

            for (i = 0; i < drone_count; i++)
                PC_MODEL_DRONE_TARGET(drones[i]) = drones[i];

            for (i = 0; i < human_count; i++) {
                for (j = i + 1; j < human_count; j++) {
                    if (car_distance[human_slots[i]][human_slots[j]] < 70 * 70) {
                        car_in_conflict[human_slots[i]] += 10;
                        car_in_conflict[human_slots[j]] += 10;
                    }
                }
            }

            for (i = 0; i < human_count; i++) {
                if (car_in_conflict[human_slots[i]] < 10) {
                    index = human_slots[i];
                    my_place = place[index];

                    index2 = index3 = -1;

                    if (my_place > 0) {
                        j = cars_in_order[my_place - 1];
                        if (PC_MODEL_DRONE_TYPE(j) == DRONE && car_in_conflict[j] == 0)
                            index2 = j;
                    }

                    if (my_place < num_active_cars - 1) {
                        j = cars_in_order[my_place + 1];
                        if (PC_MODEL_DRONE_TYPE(j) == DRONE && car_in_conflict[j] == 0)
                            index3 = j;
                    }

                    if (index2 != -1 && index3 != -1) {
                        if (car_distance[index3][index] < car_distance[index2][index])
                            index2 = index3;
                    } else if (index2 == -1)
                        index2 = index3;

                    if (index2 != -1) {
                        car_in_conflict[index]++;
                        car_in_conflict[index2]++;
                        PC_MODEL_DRONE_TARGET(index2) = index;
                    }
                }
            }

            for (indx = 0; indx < num_active_cars; indx++) {
                index = PC_MODEL_SLOT(indx);

                if (PC_MODEL_WE_CONTROL(index)) {
                    index2 = PC_MODEL_DRONE_TARGET(index);
                    my_place = place[index];


                    if (gDifficulty == 5)
                        place_scale = 0;
                    else if (my_place == 0)
                        place_scale = .5f / (num_active_cars - 1);
                    else
                        place_scale = (F32)my_place / (num_active_cars - 1);
                    diff_scale = 1.0f - (F32)gDifficulty / 5.0f;

                    if (gRace == kCircuit) {
                        temp = gTrackNum;
                        if (gBackwards)
                            high_index = 6;
                        else
                            high_index = 0;
                        diff_scale *= (F32)gCircuitCars[PC_MODEL_SLOT(index)].skill[temp + high_index] * .125f + .75f;
                    }

                    max_boost = (1.0f - 1.1f) * diff_scale + 1.1f - place_scale * ((.15f - .05f) * diff_scale + .05f);
                    min_boost = 1.0f - place_scale * ((.06f - .02f) * diff_scale + .02f);
                    max_brake = .9f - place_scale * ((.6f - .1f) * diff_scale + .1f);
                    min_brake = (.96f - 1.0f) * diff_scale + 1.0f - place_scale * ((.05f - .02f) * diff_scale + .02f);

                    if (index2 != index) {
                        delta_dist = PC_CAR_DISTANCE(index) - PC_CAR_DISTANCE(index2);

                        if (delta_dist < -20) {
                            time_scale = FoundCoin(-300, -60, delta_dist, max_boost, min_boost);
                            scale = min_brake;
                        } else {
                            if (car_in_conflict[index2] <= 1) {
                                scale = FoundCoin(200, 60, delta_dist, max_brake, min_brake);
                                time_scale = min_boost;
                            } else {
                                scale = min_brake;
                                time_scale = min_boost;
                            }
                        }
                    } else {
                        if (my_place == 0) {
                            index2 = cars_in_order[1];
                            delta_dist = PC_CAR_DISTANCE(index) - PC_CAR_DISTANCE(index2);

                            if (delta_dist < 200) {
                                time_scale = FoundCoin(200, 0, delta_dist, min_boost, max_boost);
                                scale = min_brake;
                            } else {
                                scale = FoundCoin(500, 200, delta_dist, max_brake, min_brake);
                                time_scale = min_boost;
                            }
                        } else if (my_place == num_active_cars - 1) {
                            index2 = cars_in_order[num_active_cars - 2];

                            delta_dist = PC_CAR_DISTANCE(index) - PC_CAR_DISTANCE(index2);

                            if (delta_dist < -150) {
                                time_scale = FoundCoin(-150, -300, delta_dist, min_boost, max_boost);
                                scale = min_brake;
                            } else {
                                scale = FoundCoin(-150, 0, delta_dist, min_brake, max_brake);
                                time_scale = min_boost;
                            }
                        } else {
                            index2 = cars_in_order[my_place - 1];
                            delta_dist = PC_CAR_DISTANCE(index) - PC_CAR_DISTANCE(index2);

                            index2 = cars_in_order[my_place + 1];
                            delta_dist2 = PC_CAR_DISTANCE(index) - PC_CAR_DISTANCE(index2);

                            if (delta_dist > -150 && delta_dist2 < 150) {
                                target_dist = (delta_dist + delta_dist2) / 2;

                                if (target_dist < 0) {
                                    time_scale = FoundCoin(delta_dist - target_dist, -200, delta_dist, min_boost, max_boost);
                                    scale = min_brake;
                                } else {
                                    scale = FoundCoin(delta_dist2 - target_dist, 200, delta_dist2, min_brake, max_brake);
                                    time_scale = min_boost;
                                }
                            } else {
                                for (i = 0, j = 0; i < my_place; i++) {
                                    if (PC_MODEL_DRONE_TYPE(cars_in_order[i]) == HUMAN)
                                        j++;
                                }

                                for (i = my_place, k = 0; i < num_active_cars; i++) {
                                    if (PC_MODEL_DRONE_TYPE(cars_in_order[i]) == HUMAN)
                                        k++;
                                }

                                if (j >= k) {
                                    if (delta_dist < -150) {
                                        time_scale = FoundCoin(-150, -300, delta_dist, min_boost, max_boost);
                                        scale = min_brake;
                                    } else {
                                        scale = FoundCoin(-150, 0, delta_dist, min_brake, max_brake);
                                        time_scale = min_boost;
                                    }
                                } else {
                                    if (delta_dist2 < 150) {
                                        time_scale = FoundCoin(150, 0, delta_dist2, min_boost, max_boost);
                                        scale = min_brake;
                                    } else {
                                        scale = FoundCoin(500, 150, delta_dist2, max_brake, min_brake);
                                        time_scale = min_boost;
                                    }
                                }
                            }
                        }
                    }

                    if (coast_flag || end_game_flag)
                        time_scale = min_boost;

                    if (lap_flag && PC_CAR_PLACE(human_slots[human_count - 1]) > place[index]) {
                        scale = min_brake;
                        time_scale = min_boost;
                    }

                    temp1 = PC_MODEL_DRONE_SCALE(index);

                    if (temp1 > scale) {
                        temp1 -= PC_FSCALE(.01f);
                        if (temp1 < scale)
                            temp1 = scale;
                    } else {
                        temp1 += PC_FSCALE(.01f);
                        if (temp1 > scale)
                            temp1 = scale;
                    }

                    PC_MODEL_DRONE_SCALE(index) = temp1;

                    temp1 = PC_MODEL_TIME_BOOST(index);

                    if (temp1 > time_scale) {
                        temp1 -= PC_FSCALE(.01f);
                        if (temp1 < time_scale)
                            temp1 = time_scale;
                    } else {
                        temp1 += PC_FSCALE(.01f);
                        if (temp1 > time_scale)
                            temp1 = time_scale;
                    }

                    PC_MODEL_TIME_BOOST(index) = temp1;
                }
            }
        }
    }
}


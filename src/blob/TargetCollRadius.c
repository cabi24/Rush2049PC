/* TargetCollRadius: collision test 0 of the track objects (gTargetCollFuncPtrs[0], MAT3 targets.c
 * _TargetCollRadius): a car against a sphere, for most of the small targets (in MAT3's table: coins, cones, pumps,
 * cacti, fences, signs). The arcade game's targets.c has the same test as OverlapTarget. */
#include "game/game.h"

#define mvecsub(a,b,r) {r[0]=a[0]-b[0]; r[1]=a[1]-b[1]; r[2]=a[2]-b[2];}
#define mveccopy(a,r) {r[0]=a[0]; r[1]=a[1]; r[2]=a[2];}

/* The car record's view (MAT3 tCarData) in the PC layout: only its position. */
typedef struct ProximityPlayer {
    unsigned char unknown0[8];
    float position[3];      /* 0x08 dr_pos: the position, feet */
    unsigned char unknown14[0x110 - 0x14]; struct pc_visual visuals[21]; unsigned char unknown14_308[0x380 - 0x308]; void *input; unsigned char unknown14_384[0x3B8 - 0x384];
} ProximityPlayer;

/* Does car *player_index touch the sphere of radius *radius_in at point? The car counts as a 3.5 ft sphere.
 * radial_error, if not NULL, gets the squared distance minus the squared radius (not positive: they overlap). A car
 * that isn't collidable never hits. */
int TargetCollRadius(short *player_index, float *point, float *radius_in,
                  float *radial_error)
{
    float distance_squared, gap;
    float delta[3], position[3];
    ProximityPlayer *player;
    short index;
    float radius;

    index = *player_index;
    player = PC_CAR(index);
    radius = *radius_in;
    if (!PC_MODEL_COLLIDABLE(index)) return 0;
    mveccopy(point,position);
    mvecsub(player->position,position,delta);
    distance_squared = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
    radius += 3.5f;
    gap = distance_squared - radius * radius;
    if (radial_error) *radial_error = gap;
    return (gap <= 0.0f);
}

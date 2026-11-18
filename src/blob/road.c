/* road.c: a car's contact with the road for one step (MAT3 road.c, road()).
 *
 * The first stage of the physics step (regular): the body against the world (CollideCarBody), then each tire
 * against the road under it (TireSurf: the road tree's polygon, its code and orientation), the suspensions'
 * compression from that, and last the cars against each other (collision). */
#include "game/game.h"

/* An orientation matrix: rows the x, y (the road's normal) and z axes. */
typedef struct ROADUVS { f32 fpuvs[3][3]; } ROADUVS;

/* The road step of model m: the body's forces cleared and the body collided with the world; no moving surface until
 * a tire finds one; for each tire the road under it (TireSurf gives the ground point in the tire's frame, igpos, its
 * road code and orientation), the compression (the last one less the ground's height, within +-3 ft; in the air the
 * codes become 8), the height above the road and how fast it changes (within +-40 ft/s); then the collisions with the
 * other cars. */
void road(tModelData *m)
{
    f32 BODYRWV[3], igpos[3], temp[3], last_airdist;   /* BODYRWV and temp: unused */
    s32 i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            m->bodyForce[j][i] = 0.f;

    CollideCarBody(m);
    m->movingSurface = NULL;
    for (i = 0; i < 4; ++i) {
        TireSurf(m, &m->tireRwr[i][0], igpos, &m->roadcode[i], m->roaduvs[i][0], i);
        last_airdist = m->airdist[i];
        m->suscomp[i] = m->tpcomp[i] - igpos[1];
        m->airdist[i] = -m->suscomp[i];
        if (m->suscomp[i] > 3.0f)
            m->suscomp[i] = 3.f;
        else if (m->suscomp[i] < -3.0f)
            m->suscomp[i] = -3.0f;
        m->tpcomp[i] = (m->suscomp[i] < 0.0f) ? 0.0f : m->suscomp[i];
        if (m->suscomp[i] < 0.0f)
            m->roadcode[i] = m->sviscode[i] = SURF_VIS_AIRBORNE;
        m->airvel[i] = (m->airdist[i] - last_airdist) * m->idt;
        if (m->airvel[i] < -40.0f)
            m->airvel[i] = -40.0f;
        else if (m->airvel[i] > 40.0f)
            m->airvel[i] = 40.f;
    }
    collision(m);
}

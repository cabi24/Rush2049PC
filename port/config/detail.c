/* port/config/detail.c: video.draw_distance and video.detail as the game reads them, and the scene's counts
 * (docs/port/DETAIL.md).
 *
 * main sets the settings at start (rush2049pc.ini, --set) and the settings overlay when they change (pc_detail_set);
 * the game reads them through pc_game_types.h's accessors in its scene walk, every view of every frame, so a change
 * applies from the next frame. port/gfx has its own copy for the depth range and the fog (pc_gfx_set_draw_distance,
 * pc_gfx_set_fog). At original every accessor gives the N64's value back unchanged: x * 1.0f is x, and the game
 * takes the original branch everywhere else.
 *
 * The scene's counts: the game's scene walk (particle_knot's func_8009F058, track_collision_wall, particle_system)
 * and the world's visibility (physics_float_calc) count what they draw and what each limit leaves out, per view;
 * pc_scene_report gives their means and most a view at the run's end. They only count, at every setting. */
#include <stdio.h>

#include "config.h"
#include "pc_game_types.h"
#include "pc_port.h"

static volatile int s_draw = PC_DRAW_ORIGINAL;   /* PC_DRAW_* */
static volatile int s_detail = PC_DETAIL_ORIGINAL;
static volatile int s_fog = PC_FOG_ORIGINAL;      /* PC_FOG_*: the renderer's, and SkyInit's sky dome (camera_reset) */

void pc_detail_set(int draw_distance, int detail, int fog)
{
    s_draw = draw_distance == PC_DRAW_FAR || draw_distance == PC_DRAW_MAX ? draw_distance : PC_DRAW_ORIGINAL;
    s_detail = detail == PC_DETAIL_HIGH ? PC_DETAIL_HIGH : PC_DETAIL_ORIGINAL;
    s_fog = fog == PC_FOG_FAR || fog == PC_FOG_OFF ? fog : PC_FOG_ORIGINAL;
}

int pc_fog(void)
{
    return s_fog;
}

int pc_draw_distance(void)
{
    return s_draw;
}

int pc_detail(void)
{
    return s_detail;
}

float pc_draw_reach(float d)
{
    int draw = s_draw;
    return draw == PC_DRAW_MAX ? 1e30f : draw == PC_DRAW_FAR ? d * PC_DRAW_FAR_FACTOR : d;
}

float pc_lod_distance(float d)
{
    return s_detail == PC_DETAIL_HIGH ? -1.0f : d;
}

int pc_detail_players(int players)
{
    return s_detail == PC_DETAIL_HIGH ? 1 : players;
}

/* The matrices' range (func_8009D45C, func_8009D99C, func_8009D708): a node's place against the eye, x y z in feet,
 * goes into its matrix's s15.16 translation in sixteenths of a foot, which holds |x| < 2048 ft. The power of two to
 * divide the whole matrix by (the rotation, the place and the w the node gives the vertex): 1 inside the N64's range,
 * more past it while draw_distance reaches (far: PC_DRAW_FAR_FACTOR times the range; max: any); 0 for a node the
 * setting leaves out, as the N64 does. A matrix divided by s gives every vertex s times smaller x, y, z and w: the
 * same point on the screen, the same z/w, the same fog and depth. */
int pc_draw_range_scale(float x, float y, float z)
{
    float m = x < 0.0f ? -x : x, a;
    int draw = s_draw, s = 1;
    if (x != x || y != y || z != z)   /* NaN: no comparison leaves it out on the N64 */
        return 1;
    a = y < 0.0f ? -y : y;
    if (a > m)
        m = a;
    a = z < 0.0f ? -z : z;
    if (a > m)
        m = a;
    if (m < 2048.0f)
        return 1;
    if (draw == PC_DRAW_ORIGINAL || (draw == PC_DRAW_FAR && m >= 2048.0f * PC_DRAW_FAR_FACTOR) || !(m < 1e9f))
        return 0;
    while (m >= 2048.0f * (float)s && s < (1 << 20))
        s <<= 1;
    return s;
}

/* ---------------------------------------------------------------- the scene's counts */

static const char *const s_names[PC_SCENE_COUNTS] = {
    "nodes drawn", "at a level of detail past their first", "world parts", "let through by the visibility tables",
    "hidden", "hidden in that view",
    "out of the matrices' range", "past the fog's radius", "past their last level of detail"};
static unsigned long s_views, s_walks;   /* the views of a world (a track loaded: a race), and every scene walk */
static double s_sum[PC_SCENE_COUNTS];
static unsigned s_most[PC_SCENE_COUNTS];
static unsigned s_now[PC_SCENE_COUNTS];
static int s_open, s_world;   /* a view is open; it has a world (physics_float_calc counted its parts, 0 or more) */

static void close_view(void)
{
    int k;
    if (!s_open)
        return;
    s_walks++;
    if (s_world) {   /* a view of the track's world (the arenas' tables have no parts), not a menu's model */
        s_views++;
        for (k = 0; k < PC_SCENE_COUNTS; k++) {
            s_sum[k] += s_now[k];
            if (s_now[k] > s_most[k])
                s_most[k] = s_now[k];
        }
    }
    for (k = 0; k < PC_SCENE_COUNTS; k++)
        s_now[k] = 0;
    s_open = s_world = 0;
}

void pc_scene_view(void)
{
    close_view();
    s_open = 1;
}

void pc_scene_count(int what, int n)
{
    if (what == PC_SCENE_PARTS)
        s_world = 1;
    if (what >= 0 && what < PC_SCENE_COUNTS)
        s_now[what] += (unsigned)n;
}

void pc_scene_report(uint32_t tasks, uint64_t tris, uint32_t most)
{
    int k;
    close_view();
    fprintf(stderr, "rush2049pc: scene: draw_distance %s, detail %s; %lu views of a world (of %lu drawn), a view's "
            "mean and most:", s_draw == PC_DRAW_MAX ? "max" : s_draw == PC_DRAW_FAR ? "far" : "original",
            s_detail == PC_DETAIL_HIGH ? "high" : "original", s_views, s_walks);
    for (k = 0; k < PC_SCENE_COUNTS; k++)
        fprintf(stderr, "%s %s %.1f, %u", k ? ";" : "", s_names[k], s_views ? s_sum[k] / (double)s_views : 0.0,
                s_most[k]);
    fprintf(stderr, "; %u graphics tasks, triangles %.1f a task, at most %u\n", tasks,
            tasks ? (double)tris / tasks : 0.0, most);
}

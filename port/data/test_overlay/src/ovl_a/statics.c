/* port/data/test_overlay/src/ovl_a/statics.c: runtime-image C for test_overlay_data.c. Its path has src/ovl_a/ in
 * it, so port/data/overlays.ld takes its data for image A's, as it takes the game's objects from src/ovl_a: a
 * file-local static and a pointer to it (.data, .data.rel), a BSS array, and two function-local statics, the kinds
 * the game's runtime-image C has (cursorSpeed.1, del.0, ...). */
static int counter = 5;
static float speeds[4];
static int *where = &counter;

/* Counts its calls (from 1 after each load) and moves the file's statics along. */
int ovl_test_step(void)
{
    static int calls;
    static float scale = 0.5f;
    calls++;
    scale *= 2.0f;
    speeds[calls & 3] += scale;
    (*where)++;
    where = &calls;
    return calls;
}

/* What the statics hold: counter, then speeds[1] + speeds[2]. */
int ovl_test_counter(void)
{
    return counter;
}
float ovl_test_speeds(void)
{
    return speeds[1] + speeds[2];
}

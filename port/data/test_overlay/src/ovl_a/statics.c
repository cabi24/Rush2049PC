/* port/data/test_overlay/src/ovl_a/statics.c: runtime-image C for test_overlay_data.c, a stand-in for image A's own C
 * data, of the kinds the game's has (docs/port/DATA.md 5): an initialized global, a BSS array, a pointer to data
 * (.data.rel), and the state a function keeps between calls (MAT3's function-local statics, globals in the images:
 * carscr_cursorSpeed, trackscr_mapPos, ...). */
int ovl_test_count = 5;
float ovl_test_speeds_table[4];
int *ovl_test_where = &ovl_test_count;
int ovl_test_calls;
float ovl_test_scale = 0.5f;

/* Counts its calls (from 1 after each load) and moves the data along. */
int ovl_test_step(void)
{
    ovl_test_calls++;
    ovl_test_scale *= 2.0f;
    ovl_test_speeds_table[ovl_test_calls & 3] += ovl_test_scale;
    (*ovl_test_where)++;
    ovl_test_where = &ovl_test_calls;
    return ovl_test_calls;
}

/* What the data holds: the count, then speeds[1] + speeds[2]. */
int ovl_test_counter(void)
{
    return ovl_test_count;
}
float ovl_test_speeds(void)
{
    return ovl_test_speeds_table[1] + ovl_test_speeds_table[2];
}

/* The PC layer's side of the fixture: a lookup table of names and a call into the game. */
extern s32 D_80F00100[4];
s32 helper_old_name(s32 x);

static const struct { const char *name; void *where; } known[] = {
    {"D_80F00100", D_80F00100},
};

int main(void)
{
    return helper_old_name(known[0].where != 0);
}

/* The same two names declared with two types: after the merge that is for a hand edit, not rename.py, to pick. */
extern s32 D_80F00600;
extern u32 state_word_a;

u32 alias_bits(void)
{
    return state_word_a & 1;
}

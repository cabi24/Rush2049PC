/* Two names of one address (the symbol tables have both): the maps make them one symbol. */
extern s32 D_80F00600;
extern s32 state_word_a;

s32 alias_read(void)
{
    return D_80F00600 + state_word_a;
}

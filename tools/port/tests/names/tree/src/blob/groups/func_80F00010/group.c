/* A group folder named after its function: the folder keeps its name. */
s32 func_80F00010(OldThing *thing);

s32 group_member(void)
{
    return func_80F00010(0);
}

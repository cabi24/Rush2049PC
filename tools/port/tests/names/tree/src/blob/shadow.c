/* Names of the same spelling as a global that are not the global: struct members, a macro's parameter, a
 * function's parameter, a local and its uses. Only the globals are renamed. */
extern s32 targetInfo;
extern s32 D_80F00100[4];

typedef struct Win {
    s32 targetInfo;                /* a member */
    s32 (*D_80F00100)(void);       /* a function pointer member */
} Win;

#define USE(D_80F00100) ((D_80F00100) + targetInfo)

static Win gWin = { .targetInfo = 1 };

s32 read_member(Win *w, Win v)
{
    return w->targetInfo + v.targetInfo + targetInfo;
}

s32 shadow_param(s32 D_80F00100)
{
    return D_80F00100 + 1;
}

s32 shadow_local(void)
{
    s32 targetInfo = 2;
    {
        targetInfo++;
    }
    return targetInfo + USE(3);
}

s32 global_again(void)
{
    s32 sum = targetInfo + D_80F00100[1];
    return sum;
}

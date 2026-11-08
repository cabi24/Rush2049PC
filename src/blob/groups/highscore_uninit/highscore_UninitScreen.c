typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;

extern s16 gNumPlayers;
extern s32 gNewGameState;
extern u8 highscore_inited;
extern s8 *codePane;
extern s8 *namePanes[];
extern s8 *scorePanes[2];
extern s8 *ghostPanes[3];

void RemovePane(s8 *arg0);

void highscore_UninitScreen(void)
{
    s32 i;

    if (codePane != 0) {
        RemovePane(codePane);
        codePane = 0;
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (namePanes[i] != 0) {
            RemovePane(namePanes[i]);
            namePanes[i] = 0;
        }
    }
    for (i = 0; i < 2; i++) {
        if (scorePanes[i] != 0) {
            RemovePane(scorePanes[i]);
            scorePanes[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (ghostPanes[i] != 0) {
            RemovePane(ghostPanes[i]);
            ghostPanes[i] = 0;
        }
    }
    highscore_inited = 0;
    gNewGameState = 0x800000;
}

void __standin2_func_800F1114(void)
{
    highscore_UninitScreen();
}

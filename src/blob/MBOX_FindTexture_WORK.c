/* MBOX_FindTexture_WORK.c: find a texture by name in the loaded model files (MAT3 mbox.c, MBOX_FindTexture_WORK()).
 *
 * Each model file has a table of 36-byte texture definitions sorted by name (gTexDefPtrs[model]); a texture is
 * named by a packed handle, its model's number times 1024 plus its index in that table. */
#include "game/game.h"

/* A texture definition (MAT3 tTexture; 36 bytes on the N64): only its name matters here, as the search key. */
typedef struct tTexture {
    char name[36];
} tTexture;

/* One model's table of texture definitions (MAT3 anon_texDefs_count_8). */
typedef struct TextureDefTable {
    tTexture *texDefs;
    u32 count;
} TextureDefTable;

/* What to do when the texture is missing (`flag`): nothing, warn, or stop. The messages are compiled out. */
#define MBOX_NOERR 0
#define MBOX_WARN 1
#define MBOX_FATAL 2
#define WARN_MSG(s)

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(TextureDefTable[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view

/* The texture called `name` (texcmp: the whole name) in models firstModel to lastModel (a negative bound means the
 * first or the last model), searching only the active models: returns its definition and writes its handle,
 * (model << 10) | index, to *handle; NULL and handle 0 if none has it. */
tTexture *MBOX_FindTexture_WORK(char *name, s16 *handle, s8 firstModel, s8 lastModel, s32 flag) {
    s32 i;
    char key[36];
    tTexture *found;

    found = 0;
    game_strcpy(key, name);
    if (firstModel < 0) {
        firstModel = 0;
    }
    if (lastModel < 0 || lastModel >= gNumModels) {
        lastModel = gNumModels - 1;
    }
    for (i = firstModel; i <= lastModel; i++) {
        if (!ModelActive(i)) {
            continue;
        }
        found = game_bsearch(key, gTexDefPtrs[i].texDefs, gTexDefPtrs[i].count, 36, texcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        if (flag == MBOX_WARN) {
            WARN_MSG(key);
        } else if (flag == MBOX_FATAL) {
            WARN_MSG(key);
        }
        *handle = 0;
        return 0;
    }
    *handle = (found - gTexDefPtrs[i].texDefs) | (i << 10);
    return found;
}

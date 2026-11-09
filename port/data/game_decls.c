/* port/data/game_decls.c: the game's shared declarations (include/game) compiled alone, for the data loader's
 * generator. CMake compiles this one unit with the game's flags at -O0 -g and -fno-eliminate-unused-debug-symbols,
 * so its DWARF has every datum include/game declares, used or not; tools/port/gen_data.py --decls lays each datum
 * out by that declaration alone (docs/port/DATA.md 1). The game's files may read a datum through views of their
 * own (include/game/view.h); those don't change where the data loader puts it or how it fills it. Never linked. */
#define GAME_DECLS_EXTENTS   /* the arrays' extents (EXTENT, include/game/basic.h) */
#include "game/game.h"

/* Data a game file defines with a type of its own (a tentative definition, or an initialized one): the files
 * that define them declare them, so include/game can't; their layout is still this one. */
extern f32 gDummyVtx[EXTENT(4)][3];   /* src/blob/groups/animate_arrow/group.c: defines it */
extern EnvironNode gEnvirons[EXTENT(100)];   /* src/blob/InitEnvs.c: defines it */
extern tTarget *CollisionTargetPtrs[EXTENT(30)];   /* src/blob/InitCollisionTargetPtrs.c: defines it */
extern PlayerInput gPlayers[EXTENT(4)];   /* src/blob/groups/game_sounds/InitOptions.c: defines it */
extern intptr_t gOPPFuncs[EXTENT(10)];   /* src/blob/groups/hal_time/group.c: defines it */
extern f32 weaponOffsets[EXTENT(8)][13][3];   /* src/ovl_b/groups/weapons/weapons.c: defines it */
extern ComponentLights compLight[EXTENT(2)];   /* src/ovl_a/groups/carscr/carscr.c: defines it */
extern s32 pc_audio_pool_fail_every;   /* src/blob/groups/abort_game/GetCmd.c: defines it */
extern s32 *const pc_extra_blbits;   /* src/blob/groups/zlib_deflate/group.c: defines it */
extern u8 pc_frame_dl[2][296128];   /* src/blob/groups/heap_release/initsystem_start.c: defines it */
extern f32 pc_pole_offsets[12][3];   /* src/blob/groups/resurrect/resurrect.c: defines it */
extern DmaList pc_sample_dma_list;   /* src/blob/groups/sample_dma_next/sample_dma_next.c: defines it */
extern Gfx pc_text_dl[2][2400];   /* src/blob/groups/blit/init.c: defines it */
extern const void *const pc_car_setup_data[2];   /* src/blob/groups/carsetup/group.c: defines it */

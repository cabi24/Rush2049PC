/* Game-structure type context for the game-loop cluster's m2c seeds
 * (005-game-context-bootstrap, T018/T019). Included last in autodecomp's
 * context chain (after m2c_types.h), extracted-population only (SC-005:
 * this file must never change static-population seed text).
 *
 * Scope (FR-004): every definition here exists because a named blocker in
 * the T011 cluster compile-probe (
 * research/t011-blockers.md, `autodecomp clusters --population extracted
 * --targets @`) required it, or
 * because data-model.md's committed content plan named it explicitly
 * (GState, Track_Data, the 9-entry game symbol table). Rules per
 * m2c_types.h: types/typedefs/struct defs and `extern` declarations only,
 * no function bodies; unknown fields keep `unkN` (offset N) names.
 *
 * IMPORTANT — read before adding more: several of the blockers below are
 * NOT fixable by adding more declarations here. `research/t019-stall.md`
 * documents a confirmed disasm.py symbolizer gap (pipeline/disasm.py's
 * normalize_objdump only recognizes a global reached by `lui`+`imm(reg)`;
 * it does not track `lui`+`addiu $r,$r,imm` register-pointer formation, so
 * several committed game-symbol-table entries — frame_counter, frame_dl in
 * most callers, gPlayers/input_rec1, gGameCar — stay numeric in the
 * derived asm regardless of being declared here). Declarations for those
 * symbols are kept below anyway (data-model.md requires them, and they're
 * correct/harmless — they'll take effect the moment the symbolizer gap is
 * fixed) but do not re-derive or re-verify them against a fresh cluster
 * probe expecting them to unlock anything; see the stall writeup first.
 */
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/* --- Main game state -----------------------------------------------------
 * rushtherock: game/game.h:104-117 (enum), game/globals.h:142 (gstate decl).
 * N64: GameLoop (0x800FD464) loads/stores gstate as a single byte around
 * its state dispatch (research/cluster-data-refs.md cross-check, confirmed)
 * -- the arcade's C `enum` is 4 bytes; this ROM's build packed it to u8, so
 * the extern below types it `u8`, not `GState`, to match the real access
 * width (the enum itself is kept for its member values / documentation).
 */
typedef enum GState {
    ATTRACT, TRKSEL, CARSEL, PLAYGAME, ENDGAME, GAMEOVER, HISCORE,
    PREPLAY, PREPLAY2, COUNTDOWN, NUM_GAME_STATES
} GState;

/* --- Committed game symbol table (data-model.md) --------------------------
 * research/cluster-data-refs.md is the evidence for every address; see that
 * file's per-function tables and the "Cross-check of proposed globals"
 * section for gstate/gGameCar/frame_counter confirmation.
 */
extern u8 gstate;                 /* 0x801146EC - GameLoop state dispatch, confirmed */
extern s32 frame_counter;         /* 0x80142AFC - GameLoop init/read/increment; NOT
                                    * currently symbolized (addiu gap, see header note) */
extern s32 inactiveBlit;      /* 0x801146E8 - word R/W adjacent to gstate */
extern s32 gGameState;          /* 0x801174B4 - word R/W, secondary state (110 refs) */
extern s32 gNewGameState;          /* 0x801174B8 - word R/W */

/* frame_dl (0x801497C8): attract_or_transition's struct-pattern note records
 * offsets {0x00,0x04,0x0C} read/written through it, which line up exactly
 * with OSMesgQueue's real mtqueue(0x0)/fullqueue(0x4)/first(0xC) fields
 * (OSMesgQueue is declared in m2c_types.h's context chain, included before
 * this file) -- strong evidence this is a real OSMesgQueue*, not a game-
 * specific struct. NOT currently symbolized in attract_or_transition.s or
 * Input_ProcessGameplayPad.s (both reach it via an addiu-formed pointer);
 * IS symbolized in game_loop.s/game_mode_handler.s (direct lui+lw), where
 * it compiles today as-is.
 */
extern OSMesgQueue *frame_dl;     /* 0x801497C8 */

/* --- Input records (UpdateRushControls, 0x800C997C) ---------------------------
 * rushtherock shape references (not layout truth): globals.h:143-147,213-215
 * (switch/pot globals) + modeldat.h:472-493 (fastintyp/mainintyp) describe
 * the arcade's per-control-channel scalars this N64 code plays the same
 * role as. Observed N64 shape (research/cluster-data-refs.md, UpdateRushControls
 * table): two base addresses each read/written at offsets
 * {0x01(u8),0x04(s32),0x08(s32),0x0C(s32),0x10(f32),0x14(f32)}, so this
 * struct models the surveyed prefix and the observed byte at +0x4D.
 * NOT currently symbolized (addiu gap): gPlayers/input_rec1 are formed
 * via lui+addiu in process_inputs.s, so this type does not yet reach the
 * derived seed.
 */
typedef struct {
    u8 pad00[1];   /* 0x00 */
    u8 active;     /* 0x01 - byte R */
    u8 pad02[2];   /* 0x02 */
    s32 unk04;     /* 0x04 - word W */
    s32 unk08;     /* 0x08 - word W */
    s32 unk0C;     /* 0x0C - word W */
    f32 unk10;     /* 0x10 - float W */
    f32 unk14;     /* 0x14 - float W */
    u8 _pad18[0x4D - 0x18];
    u8 unk4D;      /* 0x4D - byte R; research/cluster-data-refs.md */
} InputRecord;

extern InputRecord gPlayers;    /* 0x8014A118 */
extern InputRecord input_rec1;    /* 0x8014A164 */

/* --- UpdateRushControls's per-player lookup tables (T019 residue close-out) ---
 * Not in research/cluster-data-refs.md's UpdateRushControls table: these bases
 * are formed by a fixed lui+addiu (constant address) but then dereferenced
 * with a variable byte offset (sll+addu), which the survey pass's static
 * offset walk does not model. Evidence is a direct citation of the derived
 * assembly (third amendment, contracts/seed-derivation.md):
 * build/m2c_asm/process_inputs.s .L800C99A0-.L800C99B0 (gContLevels),
 * .L800C999C-.L800C99B4 (gContEdges), .L800C9998-.L800C99B8 (gAutoEdges),
 * .L800C9994-.L800C99BC (gContStick). Each is indexed by the active_a2/a1
 * player index (0..3): the first three via `sll $v0,$aN,2` (word stride 4,
 * one element per player), the last via `sll $tN,$aN,3` (8-byte stride,
 * two adjacent floats per player read by lwc1 at +0/+4).
 */
extern s32 gContLevels[4];
extern s32 gContEdges[4];
extern s32 gAutoEdges[4];
typedef struct {
    f32 unk0;   /* 0x0 - float R, UpdateRushControls var_v1->unk10/var_a0->unk10 */
    f32 unk4;   /* 0x4 - float R, UpdateRushControls var_v1->unk14/var_a0->unk14 */
} ContStick;
extern ContStick gContStick[4];

/* --- attract_or_transition's segment table (T019 residue close-out) -------
 * Not in research/cluster-data-refs.md (indexed via a fixed lui+addiu base
 * combined with a variable-shifted index, same class as the UpdateRushControls
 * tables above -- the survey's static offset walk does not model it).
 * Evidence, direct derived-asm citation (third amendment):
 * build/m2c_asm/attract_or_transition.s .L800EDDD4-.L800EDDD8
 * (lui $t9,0x8015 / addiu $t9,$t9,27616 forms 0x80156BE0), indexed by
 * sll $t8,$t7,7 (frame_record_index * 128, stride 0x80) then addu $t2,$t8,$t9.
 * Two fields are dereferenced off the indexed element: +0x58
 * (.L800EDDE4, lw $t6,88($t2)) is read and then itself used as a pointer
 * offset by +0x9CC0 (.L800EDE00/.L800EDE08, li $at,0x9cc0 / addu
 * $t8,$t6,$at) -- a second-level pointee, padded to that single observed
 * field per FR-004 (no wider claim about its contents); +0x7C
 * (.L800EE004, lw $a0,124($t8) with $t8 reloaded from the same base saved
 * at sp+0x48) is passed directly to osVirtualToPhysical(void *).
 * The +0x9CC0 field itself is immediately used the same way frame_dl is
 * used elsewhere in this function (assigned into frame_dl, then advanced
 * 8 bytes at a time writing word pairs) -- the same OSMesgQueue-shaped
 * 8-byte-stride idiom the frame_dl header note above already documents,
 * so it is typed OSMesgQueue here too rather than a generic word.
 */
typedef struct {
    u8 pad0000[0x9CC0];
    OSMesgQueue unk9CC0;   /* 0x9CC0 - written via frame_dl's own idiom */
} SegmentHeader;
typedef struct {
    u8 pad00[0x58];
    SegmentHeader *unk58;  /* 0x58 - pointer R */
    u8 pad5C[0x7C - 0x5C];
    void *unk7C;           /* 0x7C - pointer, arg to osVirtualToPhysical */
} SegmentTableEntry;
extern SegmentTableEntry frame_records[];  /* 0x80156BE0, element stride 0x80,
                                         * element count not evidenced */

/* --- updateGameState's typed globals (T019 residue close-out) ------
 * research/cluster-data-refs.md already lists both addresses (word R) for
 * updateGameState; the plain-s32 type from the placeholder-coverage
 * block doesn't support the pointer chains m2c infers from their actual
 * call-argument/dereference use, so each gets its own typed declaration
 * instead (per stall report's "double-pointer pointee" and "local-member
 * chain" failure classes).
 *
 * gPlayers_save (0x8014A160): the seed's call site is
 * `func_800a3424((*gPlayers_save)->unk8, 0, var_a2)` -- a double-pointer
 * pointee (contract's authorized class): gPlayers_save must be T** so
 * *gPlayers_save is T* and ->unk8 accesses T. func_800a3424's first parameter
 * is declared s32 below, so unk8 is typed s32 to match; nothing else about
 * T is evidenced.
 */
typedef struct {
    u8 pad0[8];
    s32 unk8;   /* 0x8 - updateGameState: passed as func_800a3424's
                 * first (s32) argument */
} D_8014A160_Target;
extern D_8014A160_Target **gPlayers_save;  /* 0x8014A160 */

/* gPlayerFiles_head (0x8012E6E0): a linked-list head consumed as
 * `PurgePlayerData((void **) var_s0); var_s0 = **var_s0;` -- each
 * node's first field is a pointer to the next node (self-referential),
 * matched here the same way frame_dl's OSMesgQueue-shaped idiom is: a
 * named struct whose first member is `self *next` lets m2c's `**var_s0`
 * chain (dereference-then-follow) type-check without a cast.
 */
typedef struct D_8012E6E0_Node {
    struct D_8012E6E0_Node *next;  /* 0x0 - list-traversal pointer,
                                     * see PurgePlayerData's own
                                     * void ** parameter */
} D_8012E6E0_Node;
extern D_8012E6E0_Node *gPlayerFiles_head;  /* 0x8012E6E0 */

/* --- Partial player/car struct (UpdateGame, 0x800FBF88) --------------------
 * rushtherock CAR_DATA (modeldat.h:403-467) is cited as a shape reference,
 * not layout truth (data-model.md) -- the N64 build's field order/offsets
 * are its own. Only the offsets UpdateGame actually dereferences off
 * gGameCar are declared (research/cluster-data-refs.md: base
 * 0x80152818, offsets {0x380,0x3A3}, indexed with a per-element stride of
 * 0x3B8 confirmed via the same function's `x3B8`-scaled address forms);
 * everything else is explicit padding per FR-004 (bounded scope).
 * NOT currently symbolized (addiu gap): gGameCar is formed via
 * lui+addiu wherever the cluster reaches it, so this type does not yet
 * reach the derived seed either.
 *
 * T019 residue close-out: two further offsets, evidenced directly from
 * build/m2c_asm/countdown.s (`&gGameCar[var_s0_5]` then `lw/sw
 * 232($v0)` and `lb 859($v0)`, both within the previously-blanket
 * pad000[0x380] region so must be carved out by name): +0xE8 (word,
 * read-modify-write with a `& ~8` bitmask -- a flags word) and +0x35B
 * (byte, read directly into UpdateVisuals's s8 parameter).
 */
typedef struct {
    u8 pad000[0xE8];
    s32 unkE8;              /* 0xE8 - word R/W, `& ~8` flag clear */
    u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad0EC_308[0x35B - 0x308];
    u8 unk35B;               /* 0x35B - byte R, UpdateVisuals's s8 argument */
    u8 pad35C[0x380 - 0x35C];
    void *unk380;             /* 0x380 */
    u8 pad384[0x3A3 - 0x384];
    u8 unk3A3;              /* 0x3A3 */
    u8 pad3A4[0x3B8 - 0x3A4];  /* stride to next element, per-cluster evidence only */
} GameCar;

extern GameCar gGameCar[8];   /* 0x80152818 - element count not evidenced, N64 max players */

/* --- T019 symbolization-gap additions ------------------------------------
 * Each declaration is bounded to the access pattern recorded in
 * research/cluster-data-refs.md; padding represents observed offsets, not
 * a claim that the complete game structure has been recovered.
 */
typedef struct {
    u8 pad00[0x39];
    u8 unk39;
    u8 unk3A;
    u8 unk3B;
} PlaygameSettings;
extern PlaygameSettings playgame_settings; /* 0x80114658 - updateGameState:
                                            * bytes +0x39/+0x3A/+0x3B */

typedef struct {
    u8 pad000[0x1F0];
    s32 unk1F0;
    s32 unk1F4;
    s32 unk1F8;
    s32 unk1FC;
    s32 unk200;
} CountdownObject;
typedef struct {
    u8 pad000[0x19C];
    void *unk19C;
    u8 pad1A0[0x200 - 0x1A0];
    void *unk200;
    void *unk204;
    void *unk208;
} CountdownDetail;
typedef struct {
    void *unk0;
    CountdownDetail *unk4;
    void *unk8;
    void *unkC;
    void **unk10;
} CountdownState;
extern CountdownState gText; /* 0x8017A4E0 - DrawNoControllers:
                                        * pointer fields +0x04/+0x0C/+0x10 */
extern CountdownObject *gText_single; /* 0x8017A4E4 - DrawNoControllers:
                                          * pointee +0x1F0..+0x200 */

typedef struct PadConfig_s {
    struct PadConfig_s *unk0;  /* 0x0 - pointer R; dereferenced at ->unk14
                                * elsewhere in ZOID_EndFrame
                                * (func_8008a148's first arg), matching this
                                * struct's own unk14 -- self-referential
                                * (research/cluster-data-refs.md already
                                * records 0x80140BF0/+0x0 as a pointer with
                                * sub-offsets {0x14,0x1C}, a subset of this
                                * struct's own fields) */
    struct PadConfig_s *unk4;  /* 0x4 - pointer R; dereferenced at ->unk14,
                                * ->unk10, ->unk15 (T019 residue: local
                                * `temp_v1 = var_s8->unk4` chain) --
                                * cluster-data-refs.md records 0x80140BF4/+0x4
                                * as a pointer with sub-offsets
                                * {0x10,0x12,0x14,0x15,0x16,0x18}, again a
                                * subset of this struct's own fields */
    s16 unk8;
    s16 unkA;
    s16 unkC;
    s16 unkE;
    s16 unk10;
    s16 unk12;
    u8 unk14;
    u8 unk15;
    u8 unk16;
    u8 pad17;
    s16 unk18;
    s16 unk1A;
    s16 unk1C;
    s16 unk1E;
} PadConfig;
extern PadConfig gBlitInstanceList;      /* 0x80140BF0 - ZOID_EndFrame:
                                   * config base fields +0x00..+0x1E */

/* --- ZOID_EndFrame's second config table (T019 residue) --------
 * Not in research/cluster-data-refs.md (same lui+addiu-base / variable-index
 * class the other third-amendment entries above cover). Evidence:
 * build/m2c_asm/Input_ProcessGameplayPad.s .L800A0CAC-.L800A0CB0
 * (lui $t9,0x8014 / addiu $t9,$t9,-31120 forms 0x80138670), indexed by
 * sra $t7,$t6,0xa / sll $t8,$t7,3 (stride 8) then addu $v1,$t8,$t9.
 * +0x0 (.L800A0CD0, lw $v0,0($v1)) is read and immediately dereferenced at
 * +0x14 (.L800A0CDC, lw $a0,20($v0)) and passed to func_8008a148's first
 * (s32) argument -- the same role PadConfig's own unk14 plays at every
 * other func_8008a148 call site in this function, so the pointee is typed
 * PadConfig* here too (no wider claim). +0x4 (.L800A0CC0, lw $t6,4($v1))
 * is only ever compared against zero.
 */
typedef struct {
    PadConfig *unk0;  /* 0x0 - pointer R, see above */
    s32 unk4;          /* 0x4 - word, boolean-checked */
} D_80138670_Entry;
extern D_80138670_Entry D_80138670[];  /* 0x80138670, element stride 8,
                                        * element count not evidenced */

extern s32 scheduler_retrace_count;        /* 0x8002EB64 - GameLoop: word R,
                                   * base 0x8002E8E8 + 0x27C */
extern s16 gNumPlayers;   /* 0x8014A108 - UpdateRushControls/UpdateGame/
                                   * updateGameState: halfword R/W */
extern s32 gRace;         /* 0x8014A110 - UpdateGame and
                                   * updateGameState: word R */

/* --- Complete cluster-survey placeholder coverage -----------------------
 * Every declaration in this block comes directly from the per-function
 * tables in research/cluster-data-refs.md (second 2026-07-17 gate).  Names
 * intentionally follow D_<ADDR>; widths are exactly the surveyed widths.
 */
extern f32 gRetraceRate, gRetraceTime;
extern s32 screen_width, screen_height, zbuffer;
extern u16 gRenderInProgress;
extern u8 frame_start_pending, game_frame_building, frame_end_requested;
extern s32 gLastKeyTime;
extern u8 gRestartGame, gGamePointPlayed, D_801146F0;
extern s32 D_801146F8, gPauseState;
extern u8 pause_gControllerRemoved, pause_gYes;
extern s32 gSkipToGameState;
extern u8 D_8011ED0B;
extern u16 blit_tile_lines[];  /* Input_ProcessGameplayPad.s .L800A0A74/.L800A0A88:
                             lui+addu runtime index, lhu -4852($s3) —
                             u16 array adjacent to D_8011ED0B */
extern f32 D_80123FB4, D_80123FB8, D_80123FBC, D_801242A8;
extern u8 frame_lookat_count;
extern s32 gBgColor;
extern u8 gCarTeam, gCoastFlag;
extern s32 D_80140008;
extern u16 gNumLights;
extern s32 gLights, gStuntMode, gStuntModeMins, gStuntModePoints;
extern s32 gStuntModeDeaths, gBattleMode, gBattleModeMins;
extern u8 D_80140C26;
extern s32 D_80140D70, D_80141428, gBattleModePoints;
extern u8 gOutOfTime, gRetireGame, gDeathMode;
extern s32 gAPlayerReallyFinished;
extern f32 save_server_tick_time;
extern u8 gGameStarted, gNumZoidWindows, gSofdecShown, gLicenseShown;
extern s32 blit_dl;
extern u8 gIntroMovieShown, pfsInited, lcdInited;
extern s32 frame_dl_start, pauseMenuSelect;
extern u8 D_8014B240, gCatchup, gUseCatchup;
extern s32 D_80150000;
extern u16 gNumWindows;
extern u8 gUpsideDownTrack, gTeamPoints, gNumActiveCars, D_80152F29;
extern s32 stunt_player_total_points, D_801520C4, D_80153308;
extern f32 gDeathWaitEnd, gModelTime;
extern u16 gNumLaps;
extern s32 playerInPause;
extern u8 expansion_pak_present, gControllerData, gSoftReset, frame_record_index;
extern s32 frame_dynamic, frame_matrix_cursor, video_texture_filter;
extern s32 video_color_dither, video_dither_filter, video_divot, gNumBlitInsts;
extern s32 first_unplugged_controller, zoid_poly_vertices, zoid_current_tlut, zoid_current_texture;
extern s32 zoid_tlut_mode;

/* Multi-offset bases from research/cluster-data-refs.md.  Only surveyed
 * offsets are named; every gap is explicit padding. */
typedef struct {
    u16 unk00;
    u16 unk02;
    u8 pad04[0x60 - 0x04];
    s32 unk60;
    s32 unk64;
} D_80143FD8_Record;
extern D_80143FD8_Record *gTargetList_head; /* 0x80143FD8, offsets 0,2,0x60,0x64 */

typedef struct {
    u8 pad000[0x7C6];
    u16 unk7C6;
    u8 pad7C8[0x7E8 - 0x7C8];
    u8 unk7E8;
} ModelRecord;
extern ModelRecord D_8014A250; /* 0x8014A250, offsets 0x7C6,0x7E8 */

typedef struct {
    u8 pad00[0x0C];
    u8 unk0C;
    u8 unk0D;
    u8 unk0E;
} D_80146108_Record;
extern D_80146108_Record gOptions; /* 0x80146108, bytes +0x0C..+0x0E */

/* --- Track_Data (UpdateGame, updateGameState) -------------------------
 * rushtherock: game/checkpoint.h:101-109, verbatim (platform-neutral, no
 * N64 divergence noted).
 */
typedef struct {
    F32 start_time[8];
    F32 end_time[8];
    S16 loop_chkpnt;
    S16 finish_line;
    S16 before_finish;
    S16 number_of_laps;
} Track_Data;

/* --- NewMultiBlit's own clear-record parameter (0x800B37E8) --------------
 * research/cluster-data-refs.md: "Argument a2 is a clear record pointer
 * with offsets {0x00,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x18,
 * 0x1C,0x20} (word and halfword fields)". This is the function's own
 * parameter (not a global reached through lui/addiu), so declaring
 * NewMultiBlit's real signature here is sufficient for m2c to type it --
 * confirmed fixable, no symbolizer dependency. Field widths are read
 * directly off the gaps between the evidenced offsets; 0x1C is used as a
 * callback (`temp_t7(temp_v0)` in the m2c-generated body) so it is typed a
 * function pointer, and var_s0 += 0x24 in the same body confirms the
 * struct's 0x24-byte stride.
 */
typedef struct SoundClearRecord {
    s32 unk0;                     /* 0x00 */
    s16 unk4;                     /* 0x04 */
    s16 unk6;                     /* 0x06 */
    s16 unk8;                     /* 0x08 */
    s16 unkA;                     /* 0x0A */
    s16 unkC;                     /* 0x0C */
    s16 unkE;                     /* 0x0E */
    s16 unk10;                    /* 0x10 */
    s16 unk12;                    /* 0x12 */
    s32 unk14;                    /* 0x14 */
    s32 unk18;                    /* 0x18 */
    s32 (*unk1C)(void *);         /* 0x1C - called directly in NewMultiBlit */
    s32 unk20;                    /* 0x20 */
} SoundClearRecord;

/* Partial record returned by func_800b3704. Field widths and offsets are
 * observed via m2c offsets in the NewMultiBlit seed. */
typedef struct SoundState {
    u8 _pad0[0x4];
    struct SoundState *unk4;      /* 0x04 - pointer W */
    s32 (*unk8)(void *);          /* 0x08 - callback W */
    u8 _padC[0x12 - 0xC];
    s16 unk12;                    /* 0x12 - halfword W */
    s16 unk14;                    /* 0x14 - halfword W */
    s16 unk16;                    /* 0x16 - halfword W */
    s8 unk18;                     /* 0x18 - byte W */
    u8 _pad19[0x1C - 0x19];
    s16 unk1C;                    /* 0x1C - halfword W */
    s16 unk1E;                    /* 0x1E - halfword W */
    s16 unk20;                    /* 0x20 - halfword W */
    s16 unk22;                    /* 0x22 - halfword W */
    u8 _pad24[0x28 - 0x24];
    s32 (*unk28)(void *);         /* 0x28 - callback W */
    s32 unk2C;                    /* 0x2C - word W */
    u8 _pad30[0x3C - 0x30];
    struct SoundState *unk3C;     /* 0x3C - pointer W */
} SoundState;

extern SoundState *func_800b3704(s32, s32, s32, s32); /* observed via m2c
                                                        * offset in NewMultiBlit seed */
extern SoundState *NewMultiBlit(s16 arg0, s16 arg1, SoundClearRecord *arg2, s16 arg3);

/* --- Cluster-internal call graph (game_loop.txt's own 10 targets calling
 * each other) -- every target needs the others' prototypes so callers
 * compile. All void/no-args except where the caller's own inferred call
 * site shows an argument (GameLoop calls ZOID_EndFrame(0)).
 */
extern void GameLoop(void);
extern void game_mode_handler(void);
extern void attract_or_transition(void);
extern void UpdateRushControls(void);
extern void updateGameState(void);
extern void updatePauseScreen(void);
extern void UpdateGame(void);
extern void DrawNoControllers(void);
extern void ZOID_EndFrame(s32 pad);

/* --- GameLoop's other callees (research/cluster-data-refs.md's Calls list
 * for GameLoop; m2c could not infer these return types on its own --
 * literal `?` return types are a hard parse error, see t011-blockers.md).
 */
extern void UpdateCameras(void);
/* --- return types corrected against the binary (2026-09-23) ---------------
 * Eight declarations here said `void` while the function sets $v0 at its
 * return site (cited per line, from build/m2c_asm/<fn>.s). Because hand
 * context wins over the generated layer (006 precedence), a wrong `void` is
 * authoritative: every caller that consumes the value decompiles to
 * `M2C_ERROR(/* Read from unset register $v0 *\/)`.
 *
 * Two are PROVEN by m2c's own output — HAL_SetFontInternal (called by 52
 * functions; three errors in updateGameState alone) and
 * LoadWave (two errors in audio_interrupt_handler). Correcting the
 * set moved five targets out of partial_decomp, two straight to compiled
 * (updateGameState, HAL_SetFont).
 *
 * The other six are return-site evidence only: they do leave a value in $v0,
 * but no caller consumes it today, so `void` was harmless-but-false. s32 is
 * the conservative width — the evidence proves a value is returned, not its
 * type. `python3 -m ` re-checks all of this.
 */extern s32 UpdateEnvirons(void);  /* returns: lw $v0,20($a0) */
extern void DrawBlits(void);
extern void updateFrontEnd(void);
extern void RemoveBlit(s32 sound_id);   /* also called with a pointer-typed
                                          * local in NewMultiBlit; s32
                                          * matches the concrete (non-m2c-
                                          * guessed) call site in GameLoop */

/* --- game_mode_handler's callees --- */
extern s32 HAL_CallOncePerFrameFuncs(void);  /* returns: lw $v0,%lo(D_801551F0)($s0) */
extern void UpdateFrameTimer(void);

/* --- attract_or_transition's callees --- */
extern void BlitInit(void);
extern s32 ZOID_SplitMenuScreen(s32, s32, s32, s32);
extern void world_trigger_check(void);

/* --- UpdateRushControls's callee --- */
extern void UpdateControls(void);

/* --- NewMultiBlit's other callees --- */
extern void UpdateBlit(void *);

/* --- updateGameState's callees --- */
extern void load_extra_overlay(void);           /* real signature takes s32 record
                                           * (game/maxpath.c); this call site
                                           * passes none -- matches the m2c-
                                           * inferred arity, not the arcade
                                           * one (see header note) */
extern s32 LoadWave(s32, s32, s32, s32, s32);  /* returns: m2c's own evidence -
 * audio_interrupt_handler stores its $v0 (`*sp5C = ...`) at two call sites */
extern void LoadFrontEndOverlay(s32);
extern void func_800a3424(s32, s32, s32);
extern void func_800a7480(s32, s32, u8, u8, s32, s32, s32);
extern void func_800c813c(s32, s32);      /* also called from UpdateGame */
extern void func_800c885c(void);
extern void func_800c9480(void);
extern void AudioInit(s32, s32, s32, s32, s32, f32, f32, s32);
extern void AudInit3D(s32, s32, s32, s32, s32);
extern void SetOptions(void);       /* also called from UpdateGame */
extern s32 HAL_SetFont(s32);  /* returns: lw $v0,44($sp) */
extern s32 PurgePlayerData(void **);  /* returns: lw $v0,28($sp) */
extern void HAL_PurgeFonts(void);
extern void player_mode_set(s32, s32);
extern void SetControllerActive(s32, s32);
extern void LoadFrontEndWaves(void);
extern void scene_cleanup_slots(void);
extern void AudSetMasterVol(void);              /* real signature takes s32 speed
                                            * (src/game/game.c); every call
                                            * site the cluster generates is
                                            * 0-arg -- see header note */
extern void uninitGameState(void);
extern void AudPlaySong(s32, s32);
extern void tire_compound_set(void);
extern void SaveFlush(s32);

/* --- UpdateGame's callees --- */
extern void updateHighScoreScreen(void);
extern void camera_race_setup(void);
extern void UpdateVisuals(s8);
extern s32 PrePlay(s32, s32);
extern s32 ClearRushControls(s32);  /* returns: addiu $v0,$v0,76 */
extern void updateGameOverScreen(void);
extern void func_800ab18c(s32, s32);
extern void func_800b61a8(s32, s32, s32, s32);
extern void func_800d5374(void);
extern void func_800d5828(s16);
extern void func_800d6160(void);
extern void func_800e762c(s32);
extern void func_800f7f3c(void);
extern void func_800fbe30(void);
extern void func_800fbe60(void);
extern void func_800fbf2c(void);
extern void GhostEnd(void);
extern void init_state_continue(void);
extern void modelstop(void);
extern void Update_MDrive(void);
extern s32 TextureAnimation(void);  /* returns: addiu $v0,$v0,20 */
extern void CheckTargetCollision(s16);
extern void RemoveHUD(void);
extern void PlayGame(void);
extern void *memset(void *s, s32 c, u32 n);  /* matches the convention already
                                               * used across src/game/*.c; the
                                               * one call site in UpdateGame
                                               * passes a 4th argument (an
                                               * m2c call-arg-detection
                                               * artifact -- see header note),
                                               * which stays broken here */
extern void SetCountdownTimer(f32);          /* two call sites in UpdateGame and
                                            * one in updateGameState
                                            * show 0-arg calls to the same
                                            * symbol -- an m2c call-arg-
                                            * detection artifact unrelated to
                                            * this declaration; those specific
                                            * call sites stay broken (see
                                            * header note) */

/* --- DrawNoControllers's callees --- */
extern void TB_SetTextColor(s32);
extern void func_800a4770(void *, s32);
extern s32 HAL_GetTextWidth(void *, s32);
extern s32 HAL_SetFontInternal(void);  /* returns: move $v0,$s3 */
extern void TB_DrawText(s16, s32, void *);
extern void sprintf(s8 *buf, s8 *fmt, ...);  /* matches src/game/game.c's
                                               * existing declaration for the
                                               * libc sprintf wrapper at
                                               * 0x80004990 */

/* --- ZOID_EndFrame's callees --- */
extern void func_8008705c(s32, u8, s32);
extern void BlitDraw(s32, s16, s32, s32, s32, s32);
extern void func_800878e0(s32, u8, s32);
extern void func_8008a148(s32, u8, u8, s32);
extern void func_8008a38c(u8);
extern void func_8008a3e4(s16, s16, s32, s32);
extern void func_8008a46c(s16, s16, s32, s32, void *);
extern void func_8008a644(u16);
extern void func_8009f058(s32);
extern void blit_load_texture(s32, u8, u8, u16, s32, s32, s32, s32, s32, s32, s32);

#endif

# One type per record (wave 4)

Every record the game shares between files has one declaration, in `include/game`, with every field the game uses
named and typed. A file includes `game/game.h` and uses that declaration. It doesn't declare its own copy that names
only the fields it touches and skips the rest with byte arrays.

The headers before this wave declared each datum once (PCONLY.md 2). Many records still had a different struct in each
file that reads them: the physics model, for example, had 73 declarations across the files that use it. A file that
read a datum with a different type said so with a `VIEW()` line ([view.h](../../include/game/view.h)). Those copies
had to agree on the layout, and `view_check.py`, `view_audit.py` and `layout_check.py`
([LAYOUT.md](LAYOUT.md)) checked that they did. With one declaration there is nothing to disagree.

## 1. The rules

- A record shared by two or more files is declared once, in the header of its subsystem. A record that only one file
  uses may stay in that file, declared whole.
- MAT3's names are preferred for records and fields. A field that no code reads keeps its place under a name that
  says so.
- Comments describe the record as the PC has it. Offsets are given only where they belong to a file format: a save,
  a ROM file the loader converts, or a network message.
- A file that reads a field as another type does so in its code, through a union member or a commented conversion,
  not through a second struct.

## 2. The car's physics

### 2.1 The model record

(Agent A1: the model record, MAT3's MODELDAT.)

### 2.2 The car's descriptor, its tune, its tires and the paths

(Agent A2: the physics descriptor, the tune, the tires, the reckoning, resurrection, the drones' paths.)

## 3. The game car, the scene and drawing

### 3.1 The game car

(Agent B1: the game car, MAT3's CAR_DATA.)

### 3.2 The scene

(Agent B2: visuals, matrices and vectors, windows, scene objects, polygons, skids, blasts, smoke, texture swaps.)

### 3.3 Blits and colours

(Agent B3: the blits and the one colour type.)

## 4. Players, saves, menus and sound

### 4.1 Players and their input

(Agent C1: the players, their pads and input records.)

### 4.2 Profiles, records and ghosts

The pak system's and the save code's records are declared in [save.h](../../include/game/save.h) (types.h keeps
only their names, `typedef struct tPak tPak` ..., for the headers before it). The records the game keeps in memory
have no offset comments; the two file formats, a profile's file and a ghost file's header, keep the file's offsets,
as the file's ([SAVES.md](SAVES.md)).

| Record | MAT3 | What it holds | The other tags it had |
|---|---|---|---|
| `tPak`, `tPakFile` | paksysdc.c's pak record | a controller's pak: its state flags, its libultra handle, the rumble's state, its 16 notes' directory entries | (types.h only) |
| `PakFileRecord`, reached through a `tPakHandle` | `tPS_File`, `tPS_File **` | an open pak file: its links on its pak's list (`gPS_Files`), the two functions profiles and ghosts set (`RemoveFile`, `MoveFile`), the pak, the note's number, name and extension, size, pages and data handle | `FileInfo` (carscr, trackscr, playerscr_draw, recordscr_draw), `PakFile` (playerscr, recordscr, pakscr_FindFile), `PSFile` (updateControlScreen), `Item` (circuitscr), `Resource` (modescr), `Record` and `Owner` (pakscr_draw), `Q` (FindGhosts), `Slot` (ghost_files), `tPS_File` (PS_ReadPartialFile); its handle as `PakHandle`, `PakFileHandle`, `Selection` |
| `PakErrorState`, `PakErrorFunc` | pakerr.c's dialog; paksysdc.c's `gErrorFunc` | a pak error dialog; the error callback's type | paksysdc's own `PakErrorFunc` |
| `tProfile` (tag `tPlayerProfile`) | `tPlayerProfile` | a profile in memory: its links on `gPlayerFiles` (handles), its pak file, id, name, controls, rumble and the handle of its file's data | `Profile` (9 files), `PlayerProfile` (3), `PSFile` (state), `HObject` (CheckCameraView), `Object` (SetCircuitCode, UpdateCircuit), `A` (ContinueCircuit), `PlayerState` (options_set), `HN` (rear_camera, steady_camera), `PlayerRec` (GhostStart), `OwnerBody` (circuitscr), `Owner` (modescr), `Player` (PrintScores), `OpenRecord` (pakscr_draw), types.h's `GhostPlayer`; its handle as `Handle` (4 files), `Owner` (playerscr), `MenuOwner` (circuitscr), `ListNode` (recordscr), `Player` (options_set) |
| `tSaveData` and its parts (`tRaceSettings` ... `tCarSettings`) | `tSaveData` | a profile's file on the pak (2064 bytes, the N64's revision 16): its name, options, controls, the race types' settings, the records and the circuit in progress | `SaveData` (circuitscr, recordscr_draw), `Sess` (ContinueCircuit), `Stats` (options_set), `CarCfg` and `TrackRec` (GhostStart), recordscr_draw's `RaceStats` ... `CircuitStats`; its handle as `Resource` (SetCircuitCode, UpdateCircuit, circuitscr) and `Profile` (options_set) |
| `tGhostHeader` | `tSaveHeader` | a ghost file's header (88 bytes), the compressed recording after it | `SaveHeader` (carscr) |
| `tGhostFile` | `tGhostFile` | a ghost in memory: its link on `gGhostFiles`, its pak file, track, direction, driver, time, and the handles of its recording and its file's data | `Ghost` (6 files), `Record` (save, carscr, carscr_animText), `Obj` (ghost_files), `SavedRecord` (pakscr_draw), types.h's `GhostFile`; its handle as `ReplaySlot` (GhostUpdate) |
| `tScoreOwner` | (a `tPlayerProfile **` with casts) | who set a time in the high score tables: a profile's handle, or the index of a name typed on the high score screen, or -1 | types.h's `RecordOwner`, InitRecords' `Table` |

Fields that disagreed, and how they were settled:

- **The ghost header's word at 0x30** was named `bestTime` ("the driver's best time on the track"); GhostStart writes
  the profile's `raceStats[track].totalGhosts` there, as MAT3 does into its `id`. It is `id` now.
- **The profile's id** was `u32 id0, id1` or `u8 id[8]`, a ghost's `s32 key0, key1` or `s32 id0, id1`: `u32 id[2]` and
  `u32 playerId[2]`, MAT3's. The code only compares them for equality, which is the same instruction either way.
- **The profile's name** was `name[24]` in five views (its name, controls and rumble byte as one array) and
  `name[1]` in two; the ghost's `name[14]` or `name[22]` (with its padding, or its id after it): `name[13]`.
- **The links.** A profile's `next` and `prev` are `tProfile **` (the list holds handles); a ghost's link was `ll` in
  types.h and `next` in the files: `next`, a `tGhostFile **` (the ghost list is singly linked).
- **The ghost's handles** were `void **h36` and `h40` (ghost_files), `void *datah` (carscr): `u8 **datah` (the
  decoded recording) and `tGhostHeader **header`.
- **The pak file record's fields** were `update`, `render`, `id`, `seat`, `catchup` (ghost_files), `suffix[15]`
  (pakscr_draw), `slot` (pakscr_draw's `Owner`): `RemoveFile`, `MoveFile`, `pak`, `fileNo`, `size`, `ext[11]`.
- **The save data's circuit bytes** were `option_a` and `option_b` at 1785 and 1787 (options_set) and the byte at 1785
  (trackscr's `CIRCUIT_DEATH_MODE_BYTE`): `circuit.death` and `circuit.difficulty`.
- **The profile tag** was `struct tProfile` in types.h's `PlayerTail` and `struct tPlayerProfile` in `tPadState.save`,
  two incomplete types for one record: the tag is MAT3's `tPlayerProfile` and `tProfile` its typedef, so both complete.

The data, each with its type in its header (VIEW lines removed: 43):

| Datum | Header | Was | Now | Its VIEW lines (files) and how they read it now |
|---|---|---|---|---|
| `gTotalNames` | save.h | `RecordOwner[12]` (`intptr_t names[2][5]`, `Profile **who[5]`) | `tScoreOwner[12][3][5]` | 7: `intptr_t[12][3][5]` (RemovePlayerScores, FindTempNameIdx, ControlName), `Table[12]` (InitRecords), `tProfile **[][15]` (RecordRace), `tProfile **[12][3][5]` (AddPlayerScores), `Player **[][3][5]` (PrintScores); RecordGhost used `.who[n]`. All index `[track][table][place]`, `.name` for an index, `.profile` for a handle |
| `gThisPlayer` | frontend.h | `intptr_t[4]` | `tScoreOwner[4]` | (no VIEW; RankScores and ControlName keep a handle or a name index in it) |
| `gGhosts` | save.h | `struct ReplaySlot *[4]` | `tGhostFile **[4]` | 7: `Ghost **[]` (GhostStart), `Handle[4]` (ghost_files), `intptr_t` (update_model: `(&gGhosts)[m->slot]`), `tGhostFile **[]` (PlayGame, UpdateGame), `Record **[4]` (carscr, carscr_animText); GhostUpdate read `gGhosts[i]->ghost` |
| `gNewTotalScore` | save.h | `s8[16]` | `s8[3][5]` | 4: `s8[3][5]` (ResetHighScores, RankScores, ControlName), `s8[][5]` (highscore); RecordRace and RecordGhost indexed it flat (`[shift + 5]`, `[n + 10]`): `[table][place]` |
| `gUnlockedCars`, `gUnlockedFrames` | save.h | `u8[16]`, `u8[56]` | `s8[4][13]`, `s8[4][6]` (MAT3's) | 3 each: `s8[][13]`, `s8[][6]` (CalcUnlockedStuff, IsCarUnlocked, IsFrameUnlocked, highscore); highscore_InitScreen took `&gUnlockedCars[p * 13]` and recordscr_draw `gUnlockedFrames[i]` (row 0) of the flat bytes: `gUnlockedCars[p]`, `gUnlockedFrames[0][i]` |
| `gUnlockedTracks`, `gUnlockedTrans` | save.h | `s8[1][19]`, `s8[1][8]` | `s8[4][19]`, `s8[4][8]` | (no VIEW: the data loader's extent was one row; the code reads four) |
| `gBattlePoints` | save.h | `s32` | `s8[4][4]` (MAT3's) | 3: `s8[][4]` (RecordBattlePoints, results_table), `s8[]` (RecordBattle: `[i * 4 + k]`): `[scorer][victim]` |
| `gTotal{Stunt,Battle,Obstacle,Circuit}Stats` | save.h | (their types) | (the same) | 4, recordscr_draw's own stats records: save.h's |
| `gSelectedPlayer`, `gStatPlayer` | frontend.h | `struct Owner *[4]`, `struct ListNode *` | `tProfile **[4]`, `tProfile **` | playerscr_draw, recordscr_draw: `(*h)->field` for `h->body->field` |
| `gPakFull` | frontend.h | `void *[4]` | `intptr_t[4]` (a flag, in the pointer-sized slot the data has) | playerscr's `intptr_t[]` |
| `gBlocksNeeded` | frontend.h | `char[5]` | `u8` | 3: `s8` (updateLicenseScreen), `u8` (pakscr_draw, updateWarningScreen); updateHighScoreScreen passes `&gBlocksNeeded` to CompressGhost (`u8 *blocks`, MAT3's) |
| `gErrorFunc` | save.h | `void (*)(...)` | `PakErrorFunc` | 2: the callback with an `s32` last parameter (paksysdc_flush); `intptr_t` (pak_set_error_func, which nothing calls: it converts its 32-bit argument, sign-extended as before) |
| `gEgg` | frontend.h | `union InputSequence *` (CheckForEgg's view of the egg as words) | `Egg *` | start_screen's `Egg *`; CheckForEgg's view of the egg as words (`union InputSequence`) is gone: it reads `gEgg->size`, and a step's buttons still as the egg's words, as a comment says why: its index can pass the 20 steps (the synthetic cartridge's codes are zeros, which an idle pad matches), where `levels[i]` and `edges[i]` would be out of bounds (UBSan's bounds check said so on the cheat paths) |
| `gPasswordAlphabet`, `gDefaultNames` | save.h, frontend.h | `char *`, `char *[55]` | (the same) | circuitscr's `u8 *` views |
| `obscenities` | frontend.h | `ObscenityList` | (the same) | censor's `List` |
| `gGhostPlayers`, `gRealGhostPlayers`, `gNewGhosts`, `gRealNewGhosts`, `gLimboGhosts`, `gRealLimboGhosts`, `limboGhosts`, `gPakGhosts`, `player_profile_handles`, `player_profile_records`, `gPlayerFiles` | race.h, save.h, frontend.h | `struct PlayerRec *`, `GhostPlayer`, `struct Ghost *`, `GhostFile`, `struct Record *` ..., `void **[4][16]`, `tProfile[4][16][1]`, `PlayerFileList` | `tProfile *`, `tProfile`, `tGhostFile *`, `tGhostFile`, `tGhostFile **` ..., `tProfile *[4][16]`, `tProfile[4][16]`, `AudioList` | (no VIEW) |

`gGhosts[m->slot]` (UpdateModel, [NATIVE.md](NATIVE.md) 18.4): it read `(&gGhosts)[m->slot]` through an `intptr_t`
view, which no bounds check sees. It is `gGhosts[m->slot]` of a `tGhostFile **[4]` now, so UBSan's bounds check
(`-fsanitize=bounds`, in the sanitizer builds) checks it on every path that runs a ghost race. The code bounds it
too: the test is made only in a ghost race (`gRace != kGhost ||` comes first), and a ghost race has `gNumCars =
gNumPlayers` of at most 4 (GhostStart: the players, then the ghosts in the slots after them, `n` = 2 or 4), so
`m->slot` is under 4.

The data loader lays every datum out as before: the same storage, size and alignment for every datum, the same
host offsets for every pointer-sized slot. data_symbols.txt and data.c differ in type names, the N64 sizes of the
reshaped tables (`gUnlockedCars` 16 to 52 bytes, `gUnlockedFrames` 56 to 24, `gUnlockedTracks` 19 to 76,
`gUnlockedTrans` 8 to 32, `gNewTotalScore` 16 to 15, `gBattlePoints` 4 to 16, `gBlocksNeeded` 5 to 1: each is
arena BSS, whose object runs to the next datum, so the objects are the same) and the run kinds of three own BSS
data: `gTotalNames` (its 15 slots a track were 10 `intptr_t` and 5 pointers, now 15 `intptr_t`, the union's first
member), `gPakFull` (pointers to `intptr_t`) and `gPlayerFiles` (its first word as `AudioList`'s bytes and count).
BSS is never filled from the ROM, the loader maps each N64 offset to the same host offset, and the state hash
takes pointer and `intptr_t` slots alike.

The save code reaches the records by their fields now: `groups/save/group.c` (the profiles' and ghosts' reading,
checking, creation, the save server), `ghost_files.c` and `NewGhost.c` (the ghosts), `update_model/group.c` (the
rumble request `gPaks[i].request`, the profile's rumble, the ghost header's flag), `trackscr.c`, `options_set`. The
helper macros that stood in for the fields are gone: `F()`, `SLOT_DATA` and `typedef void **Handle` (save/group.c),
`GHOST_HEADER_FLAG`, `PAK_RUMBLE_REQUEST`, `PROFILE_RUMBLE` (update_model), `CIRCUIT_DEATH_MODE_BYTE` (trackscr),
`PROFILE_SAVE`'s cast (recordscr_draw), and pc_prelude.h's `PSF_OFS`, `PROFILE_OFS`, `GHOST_OFS` (test_save.c, which
checks its mirrors of the records against the N64's layouts, keeps them for itself).

The checks: `offset_check.py --strict` no open finding (unconfirmed 8 to 5, untyped raw accesses 808 in 41 functions
to 801 in 40); `layout_check.py` the same 6 open findings as the reference build (none in these records);
`alias_check.py` none open; `bounds_check.py` none new (pairs reached through a variable 26 to 23; the three
reviewed keys whose VIEW lines went are gone from `bounds_reviewed.txt`); `census.py` 1,063 of 1,063, no pointer
warning; `n64_audit.py` no class grew; `view_check.py --sym` no live view disagrees on `gGhostFiles`, `gPS_Files`,
`pak_free_handles`, `gPlayerFiles`, `pak_file_records`, `gPaks`, `gTotalNames`, `gThisPlayer`, `gRealGhostPlayers`,
`gRealNewGhosts`, `player_profile_records` (`gGhosts`, `gSelectedPlayer`, `gStatPlayer`: no view left). offset_check's
two reviews of profile names made with `PROFILE_OFS(20)` (results_table, animBattlePoints) matched nothing any more
and are gone.

Units not `same` under objsame (each passes the synthetic gate; none changes what is loaded or stored):

| Unit | Debug | RelWithDebInfo | Why |
|---|---|---|---|
| groups/save/group.c | yes | yes | the profile's and ghost's fields for `F(p, T, PROFILE_OFS(n))` and byte offsets into the save data: the same loads and stores, with the field's offset in the instruction (`mov 0x10(%rax)`) where the raw form added it first (`add $0x10; mov (%rax)`); CreatePlayer's two list walks have a variable each (they shared one `Handle`) |
| update_model/group.c | yes | yes | the same: the rumble request `gPaks[i].request`, the profile's rumble and the ghost header's flag as fields |
| groups/save/ghost_files.c, trackscr.c | yes | no | the same: SaveGhost's file name and the circuit's death byte as fields |
| groups/save_circuit/group.c | no | yes | its checksums over the circuit's records as fields: -O2 allocates the registers otherwise |
| RecordRace.c, RecordGhost.c, RecordBattle.c, highscore_InitScreen.c, GhostStart.c | yes | yes | a table indexed by row and column (`gNewTotalScore[1][shift]`) where it was indexed flat (`[shift + 5]`), or a field where a byte offset was added (GhostStart's car settings and ghost count): the same address, computed in another order (and, where the flat index was an `int`, in 64 bits) |
| RankScores.c, ControlName.c, PrintScores.c | yes | yes | the union `tScoreOwner`: a store through `.profile` or a union copy loads its operands in another order than the `intptr_t` store with its cast; PrintScores' `.name` index takes one register move less |
| pak_set_error_func.c | yes | no | the conversion of its `s32` to the callback: `cltq` where the `intptr_t` view sign-extended with `movslq` (nothing calls it) |
| hud_text_anims/animResults.c | no | yes | -O2's instruction scheduling: the same instructions, in another order |

Numbers (src/ and nonmatching/, the 73 files this changed): local struct and union declarations 176 to 90, tags
declared in more than one file 30 (109 declarations) to 17 (58), VIEW lines 62 to 19, pad arrays 149 to 129 (most
of those left are the players' and cars' records, C1's and B1's). Over the whole game: 891 local declarations to
805, 92 duplicated tags to 78, 100 VIEW lines to 57.

Left, as other agents' records: the players' input records (`gPlayers`: `Player`, `PadState`, `InputRecord`,
`Player76` ..., C1), whose profile field is typed `tProfile **` in every file this changed; the circuit in progress
(`tCircuit`, C3), which `tSaveData.circuit` embeds as types.h's `tCircuitRecord` (`/* wave 4: tCircuit */`) and
circuitscr, ContinueCircuit, SetCircuitCode and UpdateCircuit still read through their own `struct tCircuit`; the
list headers (`tDLList`: paksysdc's and save.c's `ListHead`, the lists' owner); libultra's `OSPfs` and `OSPfsState`
(`PakFileState`, D1); `Header` in ZOID_SetPolyFlags.c and ZOID_ClearPolyFlags.c, which is a flat polygon's start
(MAT3 `tPoly`, B2), not a pak file's; `gPrevUnlockedTires` (frontend.h, read flat by highscore_InitScreen).

### 4.3 Menus, battles and circuits

(Agent C3: the menus' buttons and items, battles, circuits.)

### 4.4 Sound

(Agent C4: voices, channels, speech, sequences, sound resources.)

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

## 6. The proof

(The lead's, after the merge.)

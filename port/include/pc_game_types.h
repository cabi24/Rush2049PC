/* port/include/pc_game_types.h: game data the PC owns outright, with no N64 address (docs/port/DECOUPLE.md).
 * pc_prelude.h includes it, so every game C file sees it on TARGET_PC; the N64 build never does. Each item has one
 * PC declaration here, storage gen_data defines in data.c (port/data/decoupled.txt: outside the RDRAM window, zero
 * at startup), and accessors the game C reaches it through. A game file that uses an accessor defines its N64 side
 * itself, with its own view of the record, so the N64 tokens are the original ones:
 *     #ifndef TARGET_PC
 *     #define PC_LINK_OWNER(i) D_80153E88[i].owner
 *     #endif
 * No system headers, as for pc_prelude.h. */
#ifndef PC_GAME_TYPES_H
#define PC_GAME_TYPES_H

/* ---- The race roster: arcade gLink, N64 D_80153E88 (13 records of 8 bytes at 0x80153E88-0x80153EF0, BSS).
 * One record per car slot: who drives it and how it starts. The N64 game activates 6 slots (its MAX_LINKS: the
 * per-car arrays are 6 long), but LoadCars, sfx_position_3d and func_8039156C walk all 13, so the storage is never
 * shorter than that. PC_MAX_LINKS is the most cars a race may hold on the PC; nothing activates more than 6 yet. */
#ifndef PC_MAX_LINKS
#define PC_MAX_LINKS 8
#endif
#define PC_N64_LINK_SLOTS 13
#define PC_LINK_SLOTS (PC_MAX_LINKS > PC_N64_LINK_SLOTS ? PC_MAX_LINKS : PC_N64_LINK_SLOTS)

struct pc_link {
    unsigned char track;     /* +0 the track (CalcPolePositions, func_800EC190: D_8014978C) */
    unsigned char car;       /* +1 the car model, 0-12 (LoadCars, the car select) */
    unsigned char color[3];  /* +2 the car's three colours (the car select, LoadCars; the model record's +0x7CD) */
    unsigned char place;     /* +5 the start place (CalcPolePositions; the car record's place) */
    unsigned char flags;     /* +6 0x80 active (LINK_ACTIVE), 0x20 track and 0x10 car locked: 0xB0 in a race */
    unsigned char owner;     /* +7 0-5 a drone that node drives (0 here), 6 a human player, 7 an empty slot */
};
extern struct pc_link pc_links[PC_LINK_SLOTS];

/* The accessors: slot i's fields, the record type, the array, and the fields through a record pointer p. */
#define PC_LINK struct pc_link
#define PC_LINKS pc_links
#define PC_LINK_TRACK(i) (pc_links[i].track)
#define PC_LINK_CAR(i) (pc_links[i].car)
#define PC_LINK_COLOR0(i) (pc_links[i].color[0])
#define PC_LINK_COLOR1(i) (pc_links[i].color[1])
#define PC_LINK_COLOR2(i) (pc_links[i].color[2])
#define PC_LINK_PLACE(i) (pc_links[i].place)
#define PC_LINK_FLAGS(i) (pc_links[i].flags)
#define PC_LINK_OWNER(i) (pc_links[i].owner)
#define PC_LINKP_TRACK(p) ((p)->track)
#define PC_LINKP_PLACE(p) ((p)->place)
#define PC_LINKP_FLAGS(p) ((p)->flags)
#define PC_LINKP_OWNER(p) ((p)->owner)

#endif

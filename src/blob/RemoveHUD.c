/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * RemoveHUD() -- tear-down of the records/attract screen state (name is a historical label).
 * If the screen voice HudBlit1 is live: clear the sound handles (RemovePanes(1)), stop the
 * voice and forget it, then for each of the gNumWindows rows clear its status word resultPane[i] and
 * release the two entity slots held in the 104-byte row record at gPlayerCoin (s16 ids at +0 and
 * +52; -1 = empty) through RemoveObjectTree(id, 0, 0), marking each slot empty; finally clear
 * resultPane4. Then stop and forget the second voice HudBlit2, and in gameplay modes 6 and 4 call
 * the overlay routine at 0x8039244C. No arcade ancestor identified.
 *
 * Shaping (tiny_A121's draft was 4 words off):
 *  - `neg` is a named local holding -1 (the empty-slot marker). Retail keeps -1 in s1 and compares
 *    `beq s1,v0`; with a literal -1 (either operand order) ugen emits `beq v0,s1`. The named local
 *    gives retail's operand order.
 *  - the two loop pointers are initialised on ONE source line, status first. as1 breaks scheduling
 *    ties by source line: on one line the status `lui` lands in the blez delay slot and the addiu
 *    pair comes out row-then-status as in retail (two lines give s0,s3,s0,s3 or s3,s0,s3,s0).
 *    Proven by reassembling the edited ugen listing (asm.sh) before writing the source.
 */
typedef signed short s16;
typedef int s32;
typedef unsigned char u8;

typedef struct Voice Voice;
typedef struct RecordSlot { s16 id; u8 pad2[50]; } RecordSlot;       /* 52 bytes */
typedef struct RecordRow { RecordSlot a, b; } RecordRow;              /* 104 bytes */

extern Voice *HudBlit1;
extern Voice *HudBlit2;
extern intptr_t resultPane[];
extern s32 resultPane4;
extern s16 gNumWindows;
extern RecordRow gPlayerCoin[];
extern s32 gRace;

void RemovePanes(s32 arg0);
void RemoveBlit(Voice *voice);
void RemoveObjectTree(s16 idx, s32 freeChildren, s32 freeSiblings);
void RemoveHUDExtra(void);

void RemoveHUD(void) {
    s32 i;
    s32 id;
    s32 neg;
    RecordRow *row;
    intptr_t *status;   /* resultPane is pointer-sized on the PC */

    if (HudBlit1 != 0) {
        RemovePanes(1);
        RemoveBlit(HudBlit1);
        HudBlit1 = 0;
        neg = -1;
        i = 0;
        if (gNumWindows > 0) {
            status = resultPane; row = gPlayerCoin;
            do {
                id = row->a.id;
                *status = 0;
                if (neg != id) {
                    RemoveObjectTree((s16)id, 0, 0);
                    row->a.id = neg;
                }
                id = row->b.id;
                if (neg != id) {
                    RemoveObjectTree((s16)id, 0, 0);
                    row->b.id = neg;
                }
                i++;
                status++;
                row++;
            } while (i < gNumWindows);
        }
        resultPane4 = 0;
    }
    if (HudBlit2 != 0) {
        RemoveBlit(HudBlit2);
        HudBlit2 = 0;
    }
    if (gRace == 6 || gRace == 4) {
        RemoveHUDExtra();
    }
}

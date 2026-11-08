/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Game-over results screens, results-table family (group results_table).
 * gameover_animText (kept): results dispatcher (GC gameoverscr.c draw dispatch) + race standings table
 *   (GC drawRaceScore: rows = laps+3, cols = players+1, str1[40] = value, c[4] = color);
 * func_8010221C = GC drawMain (menu items 0..6 via validOption = func_800F84B0, then the per-mode table);
 * drawBattleStandings / drawStuntStandings (stunt/battle standings tables, GC drawBattle/StuntStandings family);
 * drawRect = GC drawRect(x, y, w, h, color) (IPA registers t1,t3,t2,t4,a2).
 * drawStuntScore = per-player stunt records table (13 rows; F30 calls it for gRace 4).
 * func_80102980 = GC drawBattleScore (battle head-to-head matrix: rows = players+2, cols = players+1;
 * F30 calls it for gRace 6) .
 * func_80100E58 = GC drawRaceStandings@gameoverscr.c (from MAT3 typed port f0e58/best_port.c and
 * drafts): rows = players+1, cols = 3 or 4 (volatile gNumCars == 1, or circuit and not place_locked),
 *   loop 1 cell backgrounds (drawRect, rank colours), loop 2 text: name column (inlined TB_DrawEllipseText =
 *   rs_DrawEllipseText with its char[160] buffer, "DRONE" for drones), header row fixed[208]/[210]/[22], best race
 *   time / best lap "%d:%02d.%03d" (font 3) or fixed[215], finishing place text. Frame 528, every anchor as retail.
 *   Helpers rs_DrawEllipseText / rs_SetJust (GC TB_DrawEllipseText / TB_SetJust): non-static, internal, inlined.
 * SOURCE FORMS (each is a spelling of the GC code; disclosed):
 *   - loop 1 computes the cell position right before drawRect (`x = ...; bgY = 36 + row * 14;` after the colour
 *     switch; GC computes them at the loop top): uopt numbers col*60 after gRankedPlayer[row-1], which gives retail's
 *     spill homes and the header-block order of the s-register values in loop 2;
 *   - the final text call is written `col * 60 - cols * 60 / 2 + 190` (= GC's x + 30): with `x + 30` the constant
 *     160 becomes a loop-invariant live-range candidate in the draw block, a 21st interferer of the cols split piece
 *     (uopt f_addadjacents needs numintf <= 20 to add the column==0 header block); proven with a split oracle;
 *   - loop 1 else-branch colour init on two lines (as GC drawStuntStandings writes it): as1 store order;
 *   - two f32 locals time/time2 for the two time columns (GC: two block-local `time`), x/y/bgY declared first.
 *   No unused locals, no pads.
 * Helpers: rt_gfx_lock/rt_gfx_unlock (the text_lock_queue font lock), rt_font_set (lock; HAL_SetFontInternal;
 *   unlock, 24 B inline area) and rt_font_set_b (lock helper + osSendMesg written out, 16 B inline area).
 *   Non-static so umerge leaves no deleted-procedure stubs; internal (not on keep). HELPER IDENTITY
 * HYPOTHESIS: F30's six font-11 sites use the rt_font_set_b shape and its three font-3 sites the
 *   rt_font_set shape; this is what gives retail's 640-byte frame and every stack offset (all nine
 *   inlined sites are code-identical, only the inline-area size differs).
 * DEVICES (disclosed):
 *  - drawBattleStandings/drawStuntStandings: written-out font sites with `if (previous) {}` holders; unused locals as
 * exact frame residual `other` (1904, D84) and block `s32 i` (D84); colour-init on four lines.
 *  - gameover_animText: two unused locals `unused1`, `unused2` = exact frame residual (retail frame 640;
 *    without them 632, with one 632 and 89 words off; 2 slots at the top of the frame, between `cell`
 *    and `y`). `cell = 60` is a real named local (it makes column*60 a propagated expression, which
 *    numbers the x web after the left/colour-table webs as in retail).
 * - func_8010221C: none (`if (!func_800F84B0(item)) continue;` replaced do {} while (0)).
 * - drawStuntScore: written-out pre-loop font site with `if (previous) {}` holder; VARIABLE REUSE:
 *    `left = 60` before the loops and `x = column*left` in the fill loop (left is reassigned in the header
 *    block): makes column*60 a propagated expression so x is numbered after `160 - width/2` as in retail
 *    (same cause as F30's `cell`; 2448's frame is exact without a new local).
 *  - func_80102980: written-out pre-loop font site (rt_gfx_lock(); previous = HAL_SetFontInternal(11);
 * osSendMesg(.); if (previous) {} -- one inlined helper level, retail's 8 B inline area);
 * colour-init on four lines in the switch arms. CELL WIDTH THROUGH A VARIABLE: the
 *    existing local `other = 60` is the cell width everywhere the 60 is a column multiplier
 *    (160-columns*other/2, x = column*other, column*other-columns*other/2+190): uopt then enters these
 *    expressions only after copy propagation, so &gPlayers[col] is numbered (and emitted) before
 *    160-columns*60/2 and the x web after the left web, as in retail (same cause as F30's `cell`).
 * Unit: the whole-program unit score F30 221C 1904 D84 2448 2980 D5C --with this file --internal <all but F30, rt_*>
 * --neighbours -> 7/7 EQUAL (+ HAL_SetFontInternal), 0 locked bodies differ. See.
 */
/* Shared source model for native results callbacks. External record gaps are
 * observed offsets/strides, not compiler-pressure stack storage. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct { u32 w0; uintptr_t w1; } Gwords;
typedef union { Gwords words; long long int force_structure_alignment; } Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_RDPPIPESYNC 0xE7
#define G_SETPRIMDEPTH 0xEE
#define G_SETPRIMCOLOR 0xFA
#define G_SETCOMBINE 0xFC
#define G_FILLRECT 0xF6

#define gDPNoParam(pkt, cmd) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = _SHIFTL(cmd, 24, 8); _g->words.w1 = 0; }
#define gDPPipeSync(pkt) gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPSetPrimDepth(pkt, z, dz) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = _SHIFTL(G_SETPRIMDEPTH, 24, 8); \
    _g->words.w1 = _SHIFTL(z, 16, 16) | _SHIFTL(dz, 0, 16); }
#define gDPSetPrimColor(pkt, m, l, r, g, b, a) { Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_SETPRIMCOLOR, 24, 8) | _SHIFTL(m, 8, 8) | _SHIFTL(l, 0, 8)); \
    _g->words.w1 = (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)); }
#define gDPSetCombineLERP_PRIM(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xFCFFFFFF; _g->words.w1 = 0xFFFDF6FB; }
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) { Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lrx), 14, 10) | _SHIFTL((lry), 2, 10)); \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10)); }

extern Gfx *blit_dl;
extern s32 blitMode;

void drawRect(s32 x, s32 y, s32 w, s32 h, u8 *color)
{
    gDPPipeSync(blit_dl++);
    gDPSetPrimDepth(blit_dl++, 0x10, 0);
    gDPSetPrimColor(blit_dl++, 0, 0, color[0], color[1], color[2], color[3]);
    gDPSetCombineLERP_PRIM(blit_dl++);
    gDPFillRectangle(blit_dl++, x, y, x + w, y + h);
    blitMode = -1;
}

typedef struct RGBA {u8 r,g,b,a;} RGBA;
typedef struct PlayerRow {u8 vehicle;u8 other[71];char **name;} PlayerRow;
/* *name is the player's profile record (save.c's SaveSlot): 64 bytes on the PC, its name at +0x20 (PROFILE_OFS,
 * pc_prelude.h; docs/port/POINTERS.md 9.10). */
/* PC: a car record seen from +0x3A3 (a name inside gGameCar): only its stride, PC_CAR_SIZE, matters here. */
typedef struct VehicleScore {s8 score;u8 other[PC_CAR_SIZE - 1];} VehicleScore;
typedef struct PlayerStats {s32 words[18];s16 laps[24];} PlayerStats;
typedef struct TextIndices {u8 other[74];u16 records;u8 other2[14];u16 menu;} TextIndices;
typedef struct TextState {void *word0;char **fixed;void *word8;TextIndices *indices;char **strings;} TextState;
extern s8 gNumActiveCars,gRankedPlayer[],gCarTeam[];
extern s8 gBattlePoints[][4];
extern s16 gNumPlayers;
extern PlayerRow gPlayers[];
extern PlayerStats gStuntPlayer[];
extern s32 playerTextColor[],gJust[2],gMainSelect,gRace;
extern u8 gameover_titlePos[];
extern char battle_score_total_format[],battle_score_cell_format[],stunt_score_total_format[],stunt_score_best_format[],stunt_score_count_format[],battle_standings_drone_name[],battle_standings_total_format[],stunt_standings_drone_name[],stunt_standings_points_format[],stunt_standings_best_format[];
/* PC: a car record seen from +0xEE (a name inside gGameCar): only its stride, PC_CAR_SIZE, matters here. */
typedef struct CarB6 {s8 lap;u8 rest[PC_CAR_SIZE - 1];} CarB6;
/* PC: a car record seen from +0xEF (a name inside gGameCar): only its stride, PC_CAR_SIZE, matters here. */
typedef struct CarB7 {s8 flag;u8 rest[PC_CAR_SIZE - 1];} CarB7;
/* PC: a car record seen from +0xF0 (a name inside gGameCar): only its stride, PC_CAR_SIZE, matters here. */
typedef struct CarF8 {f32 best;u8 rest[PC_CAR_SIZE - 4];} CarF8;
typedef struct Rec808 {s16 car;u8 rest[2054];} Rec808;
typedef struct TextIdx2 {u8 other[108];u16 cars;} TextIdx2;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern volatile s16 gNumCars;
extern s16 gNumLaps;
extern u8 gCircuitPlayer,gLap[];
extern f32 gBestLapTime[];
extern char race_standings_drone_name[],race_standings_time_format[],race_standings_lap_format[];
extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern s32 blitMode;
extern void HAL_SetTextDepth(f32);
extern void TB_SetTextColor(s32);
extern void TB_EllipseText(char *,char *,s32);
extern void TB_DrawText(s16,s16,char *);
extern s32 TB_Sprintf(char *,char *,...);
extern s32 osRecvMesg(OSMesgQueue *,void **,s32),osSendMesg(OSMesgQueue *,void *,s32);
extern s32 HAL_SetFontInternal(s32);
void drawRect(s32 x, s32 y, s32 w, s32 h, u8 *color);
extern void drawBattleStandings(void),drawStuntStandings(void),drawBattleScore(void);
void drawRaceStandings(void);
void drawStuntScore(void);
extern s32 gameover_validOption(s32),TB_GetFontLineHeight(void);
void rt_gfx_lock(void) {osRecvMesg(&text_lock_queue,0,1);}
void rt_gfx_unlock(void) {osSendMesg(&text_lock_queue,0,0);}
s32 rt_font_set(s32 font) {
    s32 previous;
    rt_gfx_lock();previous=HAL_SetFontInternal(font);rt_gfx_unlock();
    return previous;
}
s16 rs_CenterText(char *text, s16 centerX);
void rs_DrawEllipseText(s16 x, s16 y, s16 w, char *text)
{
    char str[160];
    TB_EllipseText(str, text, w);
    TB_DrawText(x, y, str);
}

void rs_SetJust(s32 h, s32 v)
{
    gJust[0] = h;
    gJust[1] = v;
}

/* PC: the finishing place's text (docs/port/MORECARS.md 7). The text bank holds six ordinals from `first`, first to
 * sixth, in the language in effect; what follows them is other text (in English, Dutch and French each ordinal's
 * suffix alone, in Italian the six again, in Spanish and German empty strings), so a 7th to 12th place read there
 * showed no number. A place past the sixth is its number followed by what the sixth ordinal has after its own number
 * (a suffix, a full stop or nothing), read from the bank at run time. */
static char *pc_place_text(char **strings, s32 first, s32 place)
{
    static char text[24];
    const char *suffix;
    s32 n = place + 1, k = 0;

    if (place < 6) {
        return strings[first + place];
    }
    suffix = strings[first + 5];
    while (*suffix >= '0' && *suffix <= '9') {
        suffix++;
    }
    if (n >= 10) {
        text[k++] = '0' + n / 10 % 10;
    }
    text[k++] = '0' + n % 10;
    while (*suffix && k < (s32)sizeof text - 1) {
        text[k++] = *suffix++;
    }
    text[k] = 0;
    return text;
}
#define PC_PLACE_TEXT(strings, first, place) pc_place_text(strings, first, place)

void drawRaceStandings(void)
{
    s32 x;
    s32 y;
    s32 bgY;
    char *str;
    char str1[40];
    s32 rows, cols;
    s32 row, col;
    f32 time;
    f32 time2;
    RGBA c;

    rows = gNumPlayers + 1;
    cols = (gNumCars == 1 || (gRace == 3 && !PC_CAR_FINISHED(gCircuitPlayer))) ? 3 : 4;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            if (row == 0 && col == 0) continue;
            if (col > 0 && row > 0) {
                c.r = 0; c.g = 0; c.b = 0; c.a = 192;
                switch (gRankedPlayer[row - 1]) {
                case 0: c.b = 128; break;
                case 1: c.r = 128; break;
                case 2: c.r = 128; c.g = 128; break;
                case 3: c.g = 128; break;
                }
            } else {
                c.r = 0; c.g = 0; c.b = 0;
                c.a = 192;
            }
            x = 160 - cols * 60 / 2 + col * 60;
            bgY = 36 + row * 14;
            drawRect(x, bgY, 60, 14, (u8 *)&c);
        }
    }
    HAL_SetTextDepth(0.0f);
    rs_SetJust(1, 1);
    rt_font_set(11);
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            x = 160 - cols * 60 / 2 + col * 60;
            y = 36 + row * 14;
            if (row == 0 && col == 0) continue;
            if (col == 0) {
                rt_font_set(11);
                if (gRankedPlayer[row - 1] < gNumPlayers) {
                    TB_SetTextColor(playerTextColor[gRankedPlayer[row - 1]]);
                    str = gPlayers[gRankedPlayer[row - 1]].name[0] + PROFILE_OFS(20);
                } else {
                    TB_SetTextColor(4);
                    str = race_standings_drone_name;
                }
                rs_DrawEllipseText(x + 30, y + 7, 60, str);
            } else if (row == 0) {
                rt_font_set(11);
                TB_SetTextColor(1);
                if (col == 1) str = gText.fixed[208];
                else if (col == 2) str = gText.fixed[210];
                else if (col == 3) str = gText.fixed[22];
            } else {
                if (col == 1) {
                    if (gLap[gRankedPlayer[row - 1]] == gNumLaps) {
                        time = PC_CAR_TIME(gRankedPlayer[row - 1]);
                        rt_font_set(3);
                        TB_SetTextColor(1);
                        TB_Sprintf(str1, race_standings_time_format, (u32)(time / 60.0f), (u32)time % 60, (u32)(time * 1000.0f) % 1000);
                        str = str1;
                    } else {
                        rt_font_set(11);
                        TB_SetTextColor(1);
                        str = gText.fixed[215];
                    }
                } else if (col == 2) {
                    if (gLap[gRankedPlayer[row - 1]] > 0) {
                        time2 = gBestLapTime[gRankedPlayer[row - 1]];
                        rt_font_set(3);
                        TB_SetTextColor(1);
                        TB_Sprintf(str1, race_standings_lap_format, (u32)(time2 / 60.0f), (u32)time2 % 60, (u32)(time2 * 1000.0f) % 1000);
                        str = str1;
                    } else {
                        rt_font_set(11);
                        TB_SetTextColor(1);
                        str = gText.fixed[215];
                    }
                } else if (col == 3) {
                    rt_font_set(11);
                    TB_SetTextColor(1);
                    str = PC_PLACE_TEXT(gText.strings, ((TextIdx2 *)gText.indices)->cars, PC_CAR_PLACE(PC_MODEL_SLOT(gRankedPlayer[row - 1])));
                }
            }
            if (col != 0) TB_DrawText(col * 60 - cols * 60 / 2 + 190, y + 7, str);
        }
    }
    HAL_SetTextDepth(-1.0f);
    rs_SetJust(0, 3);
}

void drawBattleStandings(void)
{
    s16 x;
    s32 previous;
    s16 y;
    char *text;
    char value[40];
    s32 rows,columns;
    s32 row,column,total,other;
    RGBA color;
    char label[212];
    rows=gNumActiveCars+1;
    columns=3;
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                if(column>0 && row>0) {
                    color.r=0;color.g=0;color.b=0;color.a=192;
                    switch(gCarTeam[gRankedPlayer[row-1]]) {
                    case 0:color.b=128;break;
                    case 1:color.r=128;break;
                    case 2:color.r=128;color.g=128;break;
                    case 3:color.g=128;break;
                    }
                } else {
                    color.r=0;
                    color.g=0;
                    color.b=0;
                    color.a=192;
                }
                drawRect(column*60+70,row*14+36,60,14,(u8 *)&color);
            }
        }
    }
    HAL_SetTextDepth(0.0f);
    gJust[0]=1;gJust[1]=1;
    osRecvMesg(&text_lock_queue,0,1); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                if(column==0) {
                    x=column*60+100;
                    y=row*14+43;
                    osRecvMesg(&text_lock_queue,0,1); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
                    if(gRankedPlayer[row-1]<gNumPlayers) {
                        TB_SetTextColor(playerTextColor[gCarTeam[gRankedPlayer[row-1]]]);
                        text=gPlayers[gRankedPlayer[row-1]].name[0]+PROFILE_OFS(20);
                    } else {
                        text=battle_standings_drone_name;
                        TB_SetTextColor(4);
                    }
                    TB_EllipseText(label,text,60);
                    TB_DrawText(x,y,label);
                } else if(row==0) {
                    if(column==1) {
                        TB_SetTextColor(1);
                        text=gText.fixed[130];
                    } else if(column==2) {
                        TB_SetTextColor(1);
                        text=gText.fixed[12];
                    }
                } else {
                    s32 i;
                    total=0;
                    text=value;
                    if(column==1) {
                        total=PC_CAR_POINTS(gPlayers[gRankedPlayer[row-1]].vehicle);
                    } else if(column==2) {
                        for(i=0;i<gNumPlayers;i++) {
                            total+=(gBattlePoints[i][gRankedPlayer[row-1]]>=0 ? gBattlePoints[i][gRankedPlayer[row-1]] : -gBattlePoints[i][gRankedPlayer[row-1]]);
                        }
                    }
                    TB_SetTextColor(1);
                    TB_Sprintf(value,battle_standings_total_format,total);
                }
                if(column!=0) TB_DrawText((s16)(column*60+100),(s16)(row*14+43),text);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    gJust[1]=3;gJust[0]=0;
}

void drawStuntStandings(void)
{
    s16 x;
    s32 previous;
    s16 y;
    char *text;
    char value[40];
    s32 rows,columns;
    s32 row,column,total,other;
    RGBA color;
    char label[204];
    rows=gNumActiveCars+1;
    columns=3;
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                if(column>0 && row>0) {
                    color.r=0;color.g=0;color.b=0;color.a=192;
                    switch(gRankedPlayer[row-1]) {
                    case 0:color.b=128;break;
                    case 1:color.r=128;break;
                    case 2:color.r=128;color.g=128;break;
                    case 3:color.g=128;break;
                    }
                } else {
                    color.r=0;
                    color.g=0;
                    color.b=0;
                    color.a=192;
                }
                drawRect(column*60+70,row*14+36,60,14,(u8 *)&color);
            }
        }
    }
    HAL_SetTextDepth(0.0f);
    gJust[0]=1;gJust[1]=1;
    osRecvMesg(&text_lock_queue,0,1); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                if(column==0) {
                    s32 i;
                    x=column*60+100;
                    y=row*14+43;
                    rt_font_set(11);
                    if(gRankedPlayer[row-1]<gNumPlayers) {
                        TB_SetTextColor(playerTextColor[gRankedPlayer[row-1]]);
                        text=gPlayers[gRankedPlayer[row-1]].name[0]+PROFILE_OFS(20);
                    } else {
                        text=stunt_standings_drone_name;
                        TB_SetTextColor(4);
                    }
                    TB_EllipseText(label,text,60);
                    TB_DrawText(x,y,label);
                } else if(row==0) {
                    rt_font_set(12);
                    if(column==1) {
                        TB_SetTextColor(1);
                        text=gText.fixed[130];
                    } else if(column==2) {
                        TB_SetTextColor(1);
                        text=gText.fixed[213];
                    }
                } else if(column==1) {
                    TB_SetTextColor(1);
                    text=value;
                    TB_Sprintf(value,stunt_standings_points_format,gStuntPlayer[gRankedPlayer[row-1]].words[5]);
                } else if(column==2) {
                    TB_SetTextColor(1);
                    text=value;
                    TB_Sprintf(value,stunt_standings_best_format,gStuntPlayer[gRankedPlayer[row-1]].words[2]);
                }
                if(column!=0) TB_DrawText((s16)(column*60+100),(s16)(row*14+43),text);
                osRecvMesg(&text_lock_queue,0,1); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    gJust[1]=3;gJust[0]=0;
}




void drawStuntScore(void)
{
    s32 x;
    s32 previous;
    s32 y;
    char *text;
    char value[40];
    s32 rows,columns;
    s32 row,column,left,width;
    RGBA color;
    char label[180];
    rows=13;
    columns=gNumPlayers+1;
    left=60;
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                width=columns*60;
                x=column*left;
                y=row*14+19;
                if(row>=3) {
                    if(row==3 && column==0) {
                        color.r=0;color.g=0;color.b=0;color.a=192;
                        drawRect(160-width/2+x,y,width,7,(u8 *)&color);
                    }
                    y+=7;
                }
                if(column>0 && row>0) {
                    color.r=0;color.g=0;color.b=0;color.a=192;
                    switch(column-1) {
                    case 0:color.b=128;break;
                    case 1:color.r=128;break;
                    case 2:color.r=128;color.g=128;break;
                    case 3:color.g=128;break;
                    }
                } else {
                    color.r=0;
                    color.g=0;
                    color.b=0;
                    color.a=192;
                }
                drawRect(160-width/2+x,y,60,14,(u8 *)&color);
            }
        }
    }
    HAL_SetTextDepth(0.0f);
    gJust[0]=1;gJust[1]=1;
    osRecvMesg(&text_lock_queue,0,1); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                y=row*14+19;
                if(row>=3) y+=7;
                if(row==0) {
                    s32 i;
                    TB_SetTextColor(playerTextColor[column-1]);
                    text=gPlayers[column-1].name[0]+PROFILE_OFS(20);
                    left=160-columns*60/2;
                    x=column*60;
                    TB_EllipseText(label,text,60);
                    TB_DrawText(left+x+30,y+7,label);
                } else if(column==0) {
                    rt_font_set(12);
                    if(row==1) {
                        TB_SetTextColor(1);
                        text=gText.fixed[130];
                    } else if(row==2) {
                        TB_SetTextColor(1);
                        text=gText.fixed[213];
                    } else {
                        TB_SetTextColor(1);
                        TB_Sprintf(value,gText.strings[gText.indices->records+row-3]);
                        text=value;
                    }
                } else if(row==1) {
                                        TB_SetTextColor(1);
                    TB_Sprintf(value,stunt_score_total_format,gStuntPlayer[column-1].words[5]);
                    text=value;
                } else if(row==2) {
                                        TB_SetTextColor(1);
                    TB_Sprintf(value,stunt_score_best_format,gStuntPlayer[column-1].words[2]);
                    text=value;
                } else {
                                        TB_SetTextColor(1);
                    TB_Sprintf(value,stunt_score_count_format,gStuntPlayer[column-1].laps[row-3]);
                    text=value;
                }
                if(row!=0) TB_DrawText(column*60-columns*60/2+190,y+7,text);
                rt_font_set(11);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    gJust[1]=3;gJust[0]=0;
}

/* The only native direct caller of both callbacks. */
void drawMain(void)
{
    char *text;
    s32 item,color;
    s16 x;
    s16 y;
    HAL_SetTextDepth(0.0f);
    gJust[0]=1;gJust[1]=3;
    rt_font_set(13);
    TB_SetTextColor(1);
    x=gameover_titlePos[0];y=gameover_titlePos[1];
    text=gText.fixed[63];
    TB_DrawText(x,y,text);
    y=118;
    rt_font_set(10);
    for(item=0;item<7;item++) {
        if(!gameover_validOption(item)) continue;
        {
            text=gText.strings[gText.indices->menu+item];
            if(item==0 && gRace!=4 && gRace!=6)
                text=gText.fixed[0];
            if(item==gMainSelect) color=22; else color=1;
            TB_SetTextColor(color);
            TB_DrawText(x,y,text);
            y+=TB_GetFontLineHeight();
        }
    }
    gJust[1]=3;gJust[0]=0;
    HAL_SetTextDepth(-1.0f);
    switch(gRace) {
    case 4:drawStuntStandings();break;
    case 6:drawBattleStandings();break;
    case 0:case 2:case 3:case 5:drawRaceStandings();break;
    }
}


extern s32 gGameOverState;
extern f32 best_lap_time_by_column[];
extern f32 gLapTimes[][8];
extern char race_score_time_format[],race_score_best_lap_format[],race_score_lap_time_format[];
void drawBattleScore(void)
{
    s32 x;
    s32 previous;
    s32 y;
    char *text;
    char value[40];
    s32 rows,columns;
    s32 row,column,total,other;
    RGBA color;
    char name[188];
    char label[144];
    rows=gNumPlayers+2;
    columns=gNumPlayers+1;
    other=60;
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                y=row*16-rows*16/2+110;
                x=column*other;
                if(row>=2) {
                    if(row==2 && column==0) {
                        color.r=0;color.g=0;color.b=0;color.a=192;
                        drawRect(160-columns*other/2+x,y,columns*60,8,(u8 *)&color);
                    }
                    y+=8;
                }
                if(column==0 || row==0) {
                    color.r=0;
                    color.g=0;
                    color.b=0;
                    color.a=192;
                } else if(column+1==row) {
                    color.r=0;
                    color.g=0;
                    color.b=0;
                    color.a=192;
                    switch(gCarTeam[column-1]) {
                    case 0:color.b=64;break;
                    case 1:color.r=64;break;
                    case 2:color.r=64;color.g=64;break;
                    case 3:color.g=64;break;
                    }
                } else {
                    color.r=0;
                    color.g=0;
                    color.b=0;
                    color.a=192;
                    switch(gCarTeam[column-1]) {
                    case 0:color.b=128;break;
                    case 1:color.r=128;break;
                    case 2:color.r=128;color.g=128;break;
                    case 3:color.g=128;break;
                    }
                }
                drawRect(160-columns*other/2+x,y,60,16,(u8 *)&color);
            }
        }
    }
    HAL_SetTextDepth(0.0f);
    gJust[0]=1;gJust[1]=1;
    rt_gfx_lock(); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                y=row*16-rows*16/2+110;
                if(row>=2) y+=8;
                if(row==0) {
                    TB_SetTextColor(playerTextColor[gCarTeam[column-1]]);
                    text=gPlayers[column-1].name[0]+PROFILE_OFS(20);
                    total=160-columns*other/2;
                    x=column*other;
                    TB_EllipseText(name,text,60);
                    TB_DrawText(total+x+30,y+8,name);
                } else if(column==0) {
                    if(row==1) {
                        TB_SetTextColor(1);
                        text=gText.fixed[130];
                    } else {
                        TB_SetTextColor(playerTextColor[gCarTeam[row-2]]);
                        text=gPlayers[row-2].name[0]+PROFILE_OFS(20);
                        total=160-columns*other/2;
                        x=column*other;
                        TB_EllipseText(label,text,60);
                        TB_DrawText(total+x+30,y+8,label);
                    }
                } else {
                    if(row==1) {
                        total=PC_CAR_POINTS(gPlayers[column-1].vehicle);
                        TB_SetTextColor(1);
                        TB_Sprintf(value,battle_score_total_format,total);
                        text=value;
                    } else {
                        if(column+1==row) TB_SetTextColor(4);
                        else TB_SetTextColor(1);
                        TB_Sprintf(value,battle_score_cell_format,gBattlePoints[column-1][row-2]);
                        text=value;
                    }
                }
                if(row!=0 && (column!=0 || row==1)) TB_DrawText(column*other-columns*other/2+190,y+8,text);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    gJust[1]=3;gJust[0]=0;
}
s32 rt_font_set_b(s32 font) {
    s32 previous;
    rt_gfx_lock();previous=HAL_SetFontInternal(font);osSendMesg(&text_lock_queue,0,0);
    return previous;
}
s32 gameover_animText(s32 arg0)
{
    s32 x;
    s32 cell;
    s32 unused1;
    s32 unused2;
    s32 y;
    char *text;
    char value[40];
    s32 rows,columns;
    s32 row,column,left,width;
    RGBA color;
    char label[252];
    f32 t;
    switch(gGameOverState) {
    case 1:
        drawMain();
        break;
    case 2:
        switch(gRace) {
        case 4:
            drawStuntScore();
            break;
        case 6:
            drawBattleScore();
            break;
        case 0:case 2:case 3:case 5:
            rows=gNumLaps+3;
            columns=gNumPlayers+1;
            cell=60;
            for(row=0;row<rows;row++) {
                for(column=0;column<columns;column++) {
                    if(row!=0 || column!=0) {
                        width=columns*60;
                        x=column*cell;
                        y=row*16-rows*16/2+110;
                        if(row>=3) {
                            if(row==3 && column==0) {
                                color.r=0;color.g=0;color.b=0;color.a=192;
                                drawRect(160-width/2+x,y,width,8,(u8 *)&color);
                            }
                            y+=8;
                        }
                        if(column>0 && row>0) {
                            color.r=0;color.g=0;color.b=0;color.a=192;
                            switch(column-1) {
                            case 0:color.b=128;break;
                            case 1:color.r=128;break;
                            case 2:color.r=128;color.g=128;break;
                            case 3:color.g=128;break;
                            }
                        } else {
                            color.r=0;
                            color.g=0;
                            color.b=0;
                            color.a=192;
                        }
                        drawRect(160-width/2+x,y,60,16,(u8 *)&color);
                    }
                }
            }
            HAL_SetTextDepth(0.0f);
            gJust[0]=1;gJust[1]=1;
            rt_font_set_b(11);
            for(row=0;row<rows;row++) {
                for(column=0;column<columns;column++) {
                    if(row==0 && column==0) continue;
                    {
                        y=row*16-rows*16/2+110;
                        if(row>=3) y+=8;
                        if(row==0) {
                            rt_font_set_b(11);
                            TB_SetTextColor(playerTextColor[column-1]);
                            text=gPlayers[column-1].name[0]+PROFILE_OFS(20);
                            left=160-columns*60/2;
                            x=column*cell;
                            TB_EllipseText(label,text,60);
                            TB_DrawText(left+x+30,y+8,label);
                        } else if(column==0) {
                            rt_font_set_b(11);
                            if(row==1) {
                                TB_SetTextColor(1);
                                text=gText.fixed[208];
                            } else if(row==2) {
                                TB_SetTextColor(1);
                                text=gText.fixed[210];
                            } else {
                                                                TB_SetTextColor(1);
                                TB_Sprintf(value,gText.fixed[205],row-2);
                                text=value;
                            }
                        } else if(row==1) {
                            if(gLap[gPlayers[column-1].vehicle]==gNumLaps) {
                                t=PC_CAR_TIME(gPlayers[column-1].vehicle);
                                rt_font_set(3);
                                TB_SetTextColor(1);
                                TB_Sprintf(value,race_score_time_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                rt_font_set_b(11);
                                TB_SetTextColor(1);
                                text=gText.fixed[215];
                            }
                        } else if(row==2) {
                            if(gLap[gPlayers[column-1].vehicle]>0) {
                                t=best_lap_time_by_column[column];
                                rt_font_set(3);
                                TB_SetTextColor(1);
                                TB_Sprintf(value,race_score_best_lap_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                rt_font_set_b(11);
                                TB_SetTextColor(1);
                                text=gText.fixed[215];
                            }
                        } else {
                            if(row-3<gLap[gPlayers[column-1].vehicle]) {
                                t=gLapTimes[column-1][row-3];
                                rt_font_set(3);
                                TB_SetTextColor(1);
                                TB_Sprintf(value,race_score_lap_time_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                rt_font_set_b(11);
                                TB_SetTextColor(1);
                                text=gText.fixed[215];
                            }
                        }
                        if(row!=0) TB_DrawText(column*60-columns*60/2+190,y+8,text);
                    }
                }
            }
            HAL_SetTextDepth(-1.0f);
            gJust[1]=3;gJust[0]=0;
            break;
        }
        break;
    }
    return 1;
}


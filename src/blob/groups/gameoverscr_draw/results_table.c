/* The game-over screen's drawing (MAT3 gameoverscr.c drawRect, drawMain, the draw*Standings and draw*Score
 * functions and animText): gameover_animText is the screen's text blit callback. The menu page shows the
 * menu and the race's standings; the score page the race's full table: lap by lap for a race, the stunt points or
 * the battle points. Each player's cells are tinted in the player's colour (blue, red, yellow, green). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

typedef union { Gwords words; long long int force_structure_alignment; } Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))

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

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

/* Fill the screen rectangle (x, y, w, h) with color (r, g, b, a) straight into the blit display list. */
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
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): its name at 0x20 (N64 0x14). */
typedef struct PlayerProfile {void *next, *prev, *fileh; u32 id[2]; char name[13];} PlayerProfile;
/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct PlayerRow {u8 slot;u8 other[71];PlayerProfile **profile;} PlayerRow;

typedef struct PlayerStats {s32 words[18];s16 laps[24];} PlayerStats;
#define TEXT_STUNT_RECORDS 37       /* gText.tab[]: the stunt score page's record rows */

VIEW(s8[][4], gBattlePoints);
#define gBattlePoints gBattlePoints_view

extern PlayerRow gPlayers[];
VIEW(PlayerStats[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view
VIEW(s32[2], gJust);
#define gJust gJust_view

VIEW(OSMesgQueue, text_lock_queue);
#define text_lock_queue text_lock_queue_view

extern s32 osRecvMesg(OSMesgQueue *,void **,s32),osSendMesg(OSMesgQueue *,void *,s32);

/* Draw text at (x, y), cut to w pixels with an ellipsis if it is wider. */
void rs_DrawEllipseText(s16 x, s16 y, s16 w, char *text)
{
    char str[160];
    TB_EllipseText(str, text, w);
    TB_DrawText(x, y, str);
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

/* The race's standings under the menu: a row per player in finishing order (the profile's name, or the drone's)
 * with its time, best lap and place; without the place column for a lone car or an unfinished circuit race. */
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
    cols = (gNumCars == 1 || (gRace == kCircuit && !PC_CAR_FINISHED(gCircuitPlayer))) ? 3 : 4;
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
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    HAL_SetFont(11);
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            x = 160 - cols * 60 / 2 + col * 60;
            y = 36 + row * 14;
            if (row == 0 && col == 0) continue;
            if (col == 0) {
                HAL_SetFont(11);
                if (gRankedPlayer[row - 1] < gNumPlayers) {
                    TB_SetTextColor(playerTextColor[gRankedPlayer[row - 1]]);
                    str = (*gPlayers[gRankedPlayer[row - 1]].profile)->name;
                } else {
                    TB_SetTextColor(4);
                    str = race_standings_drone_name;
                }
                rs_DrawEllipseText(x + 30, y + 7, 60, str);
            } else if (row == 0) {
                HAL_SetFont(11);
                TB_SetTextColor(kTB_WhiteText);
                if (col == 1) str = gText.single[208];
                else if (col == 2) str = gText.single[210];
                else if (col == 3) str = gText.single[22];
            } else {
                if (col == 1) {
                    if (gLap[gRankedPlayer[row - 1]] == gNumLaps) {
                        time = PC_CAR_TIME(gRankedPlayer[row - 1]);
                        HAL_SetFont(3);
                        TB_SetTextColor(kTB_WhiteText);
                        TB_Sprintf(str1, race_standings_time_format, (u32)(time / 60.0f), (u32)time % 60, (u32)(time * 1000.0f) % 1000);
                        str = str1;
                    } else {
                        HAL_SetFont(11);
                        TB_SetTextColor(kTB_WhiteText);
                        str = gText.single[215];
                    }
                } else if (col == 2) {
                    if (gLap[gRankedPlayer[row - 1]] > 0) {
                        time2 = gBestLapTime[gRankedPlayer[row - 1]];
                        HAL_SetFont(3);
                        TB_SetTextColor(kTB_WhiteText);
                        TB_Sprintf(str1, race_standings_lap_format, (u32)(time2 / 60.0f), (u32)time2 % 60, (u32)(time2 * 1000.0f) % 1000);
                        str = str1;
                    } else {
                        HAL_SetFont(11);
                        TB_SetTextColor(kTB_WhiteText);
                        str = gText.single[215];
                    }
                } else if (col == 3) {
                    HAL_SetFont(11);
                    TB_SetTextColor(kTB_WhiteText);
                    str = PC_PLACE_TEXT(gText.taba, gText.tab[TEXT_PLACES], PC_CAR_PLACE(PC_MODEL_SLOT(gRankedPlayer[row - 1])));
                }
            }
            if (col != 0) TB_DrawText(col * 60 - cols * 60 / 2 + 190, y + 7, str);
        }
    }
    HAL_SetTextDepth(-1.0f);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
}

/* A battle's standings under the menu: each player's battle points. */
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
                        text=(*gPlayers[gRankedPlayer[row-1]].profile)->name;
                    } else {
                        text=battle_standings_drone_name;
                        TB_SetTextColor(4);
                    }
                    TB_EllipseText(label,text,60);
                    TB_DrawText(x,y,label);
                } else if(row==0) {
                    if(column==1) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[130];
                    } else if(column==2) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[12];
                    }
                } else {
                    s32 i;
                    total=0;
                    text=value;
                    if(column==1) {
                        total=PC_CAR_POINTS(gPlayers[gRankedPlayer[row-1]].slot);
                    } else if(column==2) {
                        for(i=0;i<gNumPlayers;i++) {
                            total+=(gBattlePoints[i][gRankedPlayer[row-1]]>=0 ? gBattlePoints[i][gRankedPlayer[row-1]] : -gBattlePoints[i][gRankedPlayer[row-1]]);
                        }
                    }
                    TB_SetTextColor(kTB_WhiteText);
                    TB_Sprintf(value,battle_standings_total_format,total);
                }
                if(column!=0) TB_DrawText((s16)(column*60+100),(s16)(row*14+43),text);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    gJust[1]=3;gJust[0]=0;
}

/* A stunt race's standings under the menu: each player's points and best stunt. */
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
                    HAL_SetFont(11);
                    if(gRankedPlayer[row-1]<gNumPlayers) {
                        TB_SetTextColor(playerTextColor[gRankedPlayer[row-1]]);
                        text=(*gPlayers[gRankedPlayer[row-1]].profile)->name;
                    } else {
                        text=stunt_standings_drone_name;
                        TB_SetTextColor(4);
                    }
                    TB_EllipseText(label,text,60);
                    TB_DrawText(x,y,label);
                } else if(row==0) {
                    HAL_SetFont(12);
                    if(column==1) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[130];
                    } else if(column==2) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[213];
                    }
                } else if(column==1) {
                    TB_SetTextColor(kTB_WhiteText);
                    text=value;
                    TB_Sprintf(value,stunt_standings_points_format,gStuntPlayer[gRankedPlayer[row-1]].words[5]);
                } else if(column==2) {
                    TB_SetTextColor(kTB_WhiteText);
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

/* A stunt race's score page: each player's totals and record rows. */
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
                    text=(*gPlayers[column-1].profile)->name;
                    left=160-columns*60/2;
                    x=column*60;
                    TB_EllipseText(label,text,60);
                    TB_DrawText(left+x+30,y+7,label);
                } else if(column==0) {
                    HAL_SetFont(12);
                    if(row==1) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[130];
                    } else if(row==2) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[213];
                    } else {
                        TB_SetTextColor(kTB_WhiteText);
                        TB_Sprintf(value,gText.taba[gText.tab[TEXT_STUNT_RECORDS]+row-3]);
                        text=value;
                    }
                } else if(row==1) {
                                        TB_SetTextColor(kTB_WhiteText);
                    TB_Sprintf(value,stunt_score_total_format,gStuntPlayer[column-1].words[5]);
                    text=value;
                } else if(row==2) {
                                        TB_SetTextColor(kTB_WhiteText);
                    TB_Sprintf(value,stunt_score_best_format,gStuntPlayer[column-1].words[2]);
                    text=value;
                } else {
                                        TB_SetTextColor(kTB_WhiteText);
                    TB_Sprintf(value,stunt_score_count_format,gStuntPlayer[column-1].laps[row-3]);
                    text=value;
                }
                if(row!=0) TB_DrawText(column*60-columns*60/2+190,y+7,text);
                HAL_SetFont(11);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    gJust[1]=3;gJust[0]=0;
}

/* The only native direct caller of both callbacks. */
/* The menu page: the title, the offered rows (the selected one flashing; the score row is labelled differently
 * except after a stunt race or a battle), then the race's standings. */
void drawMain(void)
{
    char *text;
    s32 item,color;
    s16 x;
    s16 y;
    HAL_SetTextDepth(0.0f);
    gJust[0]=1;gJust[1]=3;
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    x=gameover_titlePos[0];y=gameover_titlePos[1];
    text=gText.single[63];
    TB_DrawText(x,y,text);
    y=118;
    HAL_SetFont(10);
    for(item=0;item<kGameOverMainMenuCount;item++) {
        if(!gameover_validOption(item)) continue;
        {
            text=gText.taba[gText.tab[TEXT_GAME_OVER_MENU]+item];
            if(item==kGameOverMainMenuScore && gRace!=kStunt && gRace!=kBattle)
                text=gText.single[0];
            if(item==gMainSelect) color=kTB_FlashText; else color=kTB_WhiteText;
            TB_SetTextColor(color);
            TB_DrawText(x,y,text);
            y+=TB_GetFontLineHeight();
        }
    }
    gJust[1]=3;gJust[0]=0;
    HAL_SetTextDepth(-1.0f);
    switch(gRace) {
    case kStunt:drawStuntStandings();break;
    case kBattle:drawBattleStandings();break;
    case kOneRace:case kGhost:case kCircuit:case kObstacle:drawRaceStandings();break;
    }
}

/* A battle's score page: who scored on whom. */
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
    text_lock(); previous = HAL_SetFontInternal(11); osSendMesg(&text_lock_queue,0,0); if (previous) {}
    for(row=0;row<rows;row++) {
        for(column=0;column<columns;column++) {
            if(row!=0 || column!=0) {
                y=row*16-rows*16/2+110;
                if(row>=2) y+=8;
                if(row==0) {
                    TB_SetTextColor(playerTextColor[gCarTeam[column-1]]);
                    text=(*gPlayers[column-1].profile)->name;
                    total=160-columns*other/2;
                    x=column*other;
                    TB_EllipseText(name,text,60);
                    TB_DrawText(total+x+30,y+8,name);
                } else if(column==0) {
                    if(row==1) {
                        TB_SetTextColor(kTB_WhiteText);
                        text=gText.single[130];
                    } else {
                        TB_SetTextColor(playerTextColor[gCarTeam[row-2]]);
                        text=(*gPlayers[row-2].profile)->name;
                        total=160-columns*other/2;
                        x=column*other;
                        TB_EllipseText(label,text,60);
                        TB_DrawText(total+x+30,y+8,label);
                    }
                } else {
                    if(row==1) {
                        total=PC_CAR_POINTS(gPlayers[column-1].slot);
                        TB_SetTextColor(kTB_WhiteText);
                        TB_Sprintf(value,battle_score_total_format,total);
                        text=value;
                    } else {
                        if(column+1==row) TB_SetTextColor(4);
                        else TB_SetTextColor(kTB_WhiteText);
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
/* The screen's text blit callback: the menu page, or the score page: for a race, a table of each lap's time per
 * player under the totals' rows. Always 1. */
s32 gameover_animText(s32 blit)
{
    s32 x;
    s32 cell;
    s32 y;
    char *text;
    char value[40];
    s32 rows,columns;
    s32 row,column,left,width;
    RGBA color;
    char label[252];
    f32 t;
    switch(gGameOverState) {
    case kGameOverStateMain:
        drawMain();
        break;
    case kGameOverStateScore:
        switch(gRace) {
        case kStunt:
            drawStuntScore();
            break;
        case kBattle:
            drawBattleScore();
            break;
        case kOneRace:case kGhost:case kCircuit:case kObstacle:
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
            HAL_SetFont(11);
            for(row=0;row<rows;row++) {
                for(column=0;column<columns;column++) {
                    if(row==0 && column==0) continue;
                    {
                        y=row*16-rows*16/2+110;
                        if(row>=3) y+=8;
                        if(row==0) {
                            HAL_SetFont(11);
                            TB_SetTextColor(playerTextColor[column-1]);
                            text=(*gPlayers[column-1].profile)->name;
                            left=160-columns*60/2;
                            x=column*cell;
                            TB_EllipseText(label,text,60);
                            TB_DrawText(left+x+30,y+8,label);
                        } else if(column==0) {
                            HAL_SetFont(11);
                            if(row==1) {
                                TB_SetTextColor(kTB_WhiteText);
                                text=gText.single[208];
                            } else if(row==2) {
                                TB_SetTextColor(kTB_WhiteText);
                                text=gText.single[210];
                            } else {
                                                                TB_SetTextColor(kTB_WhiteText);
                                TB_Sprintf(value,gText.single[205],row-2);
                                text=value;
                            }
                        } else if(row==1) {
                            if(gLap[gPlayers[column-1].slot]==gNumLaps) {
                                t=PC_CAR_TIME(gPlayers[column-1].slot);
                                HAL_SetFont(3);
                                TB_SetTextColor(kTB_WhiteText);
                                TB_Sprintf(value,race_score_time_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                HAL_SetFont(11);
                                TB_SetTextColor(kTB_WhiteText);
                                text=gText.single[215];
                            }
                        } else if(row==2) {
                            if(gLap[gPlayers[column-1].slot]>0) {
                                t=gBestLapTime[column-1];   /* column 1 is player 0 */
                                HAL_SetFont(3);
                                TB_SetTextColor(kTB_WhiteText);
                                TB_Sprintf(value,race_score_best_lap_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                HAL_SetFont(11);
                                TB_SetTextColor(kTB_WhiteText);
                                text=gText.single[215];
                            }
                        } else {
                            if(row-3<gLap[gPlayers[column-1].slot]) {
                                t=gLapTimes[column-1][row-3];
                                HAL_SetFont(3);
                                TB_SetTextColor(kTB_WhiteText);
                                TB_Sprintf(value,race_score_lap_time_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                HAL_SetFont(11);
                                TB_SetTextColor(kTB_WhiteText);
                                text=gText.single[215];
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

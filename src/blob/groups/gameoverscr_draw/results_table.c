#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

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

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

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

typedef struct PlayerStats {s32 words[18];s16 laps[24];} PlayerStats;
typedef struct TextIndices {u8 other[74];u16 records;u8 other2[14];u16 menu;} TextIndices;
typedef struct TextState {void *word0;char **fixed;void *word8;TextIndices *indices;char **strings;} TextState;

VIEW(s8[][4], gBattlePoints);
#define gBattlePoints gBattlePoints_view

extern PlayerRow gPlayers[];
VIEW(PlayerStats[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view
VIEW(s32[2], gJust);
#define gJust gJust_view

typedef struct TextIdx2 {u8 other[108];u16 cars;} TextIdx2;

VIEW(OSMesgQueue, text_lock_queue);
#define text_lock_queue text_lock_queue_view
VIEW(TextState, gText);
#define gText gText_view

extern s32 osRecvMesg(OSMesgQueue *,void **,s32),osSendMesg(OSMesgQueue *,void *,s32);

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
    TB_SetJust(1, 1);
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
                    str = gPlayers[gRankedPlayer[row - 1]].name[0] + PROFILE_OFS(20);
                } else {
                    TB_SetTextColor(4);
                    str = race_standings_drone_name;
                }
                rs_DrawEllipseText(x + 30, y + 7, 60, str);
            } else if (row == 0) {
                HAL_SetFont(11);
                TB_SetTextColor(1);
                if (col == 1) str = gText.fixed[208];
                else if (col == 2) str = gText.fixed[210];
                else if (col == 3) str = gText.fixed[22];
            } else {
                if (col == 1) {
                    if (gLap[gRankedPlayer[row - 1]] == gNumLaps) {
                        time = PC_CAR_TIME(gRankedPlayer[row - 1]);
                        HAL_SetFont(3);
                        TB_SetTextColor(1);
                        TB_Sprintf(str1, race_standings_time_format, (u32)(time / 60.0f), (u32)time % 60, (u32)(time * 1000.0f) % 1000);
                        str = str1;
                    } else {
                        HAL_SetFont(11);
                        TB_SetTextColor(1);
                        str = gText.fixed[215];
                    }
                } else if (col == 2) {
                    if (gLap[gRankedPlayer[row - 1]] > 0) {
                        time2 = gBestLapTime[gRankedPlayer[row - 1]];
                        HAL_SetFont(3);
                        TB_SetTextColor(1);
                        TB_Sprintf(str1, race_standings_lap_format, (u32)(time2 / 60.0f), (u32)time2 % 60, (u32)(time2 * 1000.0f) % 1000);
                        str = str1;
                    } else {
                        HAL_SetFont(11);
                        TB_SetTextColor(1);
                        str = gText.fixed[215];
                    }
                } else if (col == 3) {
                    HAL_SetFont(11);
                    TB_SetTextColor(1);
                    str = PC_PLACE_TEXT(gText.strings, ((TextIdx2 *)gText.indices)->cars, PC_CAR_PLACE(PC_MODEL_SLOT(gRankedPlayer[row - 1])));
                }
            }
            if (col != 0) TB_DrawText(col * 60 - cols * 60 / 2 + 190, y + 7, str);
        }
    }
    HAL_SetTextDepth(-1.0f);
    TB_SetJust(0, 3);
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
                    HAL_SetFont(11);
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
                    HAL_SetFont(12);
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
                    HAL_SetFont(12);
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
                HAL_SetFont(11);
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
    HAL_SetFont(13);
    TB_SetTextColor(1);
    x=gameover_titlePos[0];y=gameover_titlePos[1];
    text=gText.fixed[63];
    TB_DrawText(x,y,text);
    y=118;
    HAL_SetFont(10);
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
                            text=gPlayers[column-1].name[0]+PROFILE_OFS(20);
                            left=160-columns*60/2;
                            x=column*cell;
                            TB_EllipseText(label,text,60);
                            TB_DrawText(left+x+30,y+8,label);
                        } else if(column==0) {
                            HAL_SetFont(11);
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
                                HAL_SetFont(3);
                                TB_SetTextColor(1);
                                TB_Sprintf(value,race_score_time_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                HAL_SetFont(11);
                                TB_SetTextColor(1);
                                text=gText.fixed[215];
                            }
                        } else if(row==2) {
                            if(gLap[gPlayers[column-1].vehicle]>0) {
                                t=gBestLapTime[column-1];   /* column 1 is player 0 */
                                HAL_SetFont(3);
                                TB_SetTextColor(1);
                                TB_Sprintf(value,race_score_best_lap_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                HAL_SetFont(11);
                                TB_SetTextColor(1);
                                text=gText.fixed[215];
                            }
                        } else {
                            if(row-3<gLap[gPlayers[column-1].vehicle]) {
                                t=gLapTimes[column-1][row-3];
                                HAL_SetFont(3);
                                TB_SetTextColor(1);
                                TB_Sprintf(value,race_score_lap_time_format,(u32)(t/60.0f),(u32)t%60,(u32)(t*1000.0f)%1000);
                                text=value;
                            } else {
                                HAL_SetFont(11);
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


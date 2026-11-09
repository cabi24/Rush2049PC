/* The player (profile select) screen's drawing (MAT3 playerscr.c animText and its draw* functions): each window's
 * text over its 3D buttons, by the window's state. playerscr_animText is the screen's text blit callback. */
#include "game/game.h"

typedef TextColor Color4;   /* the flashing colours (include/game's TextColor) */
/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10): pak is
 * the pak it is on. */
typedef struct FileInfo { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak; } FileInfo;
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): the links of gPlayerFiles, its pak
 * file (NULL for a guest), its id, its name. */
typedef struct Profile Profile;
struct Profile { Profile **next,**previous; FileInfo **fileh; u8 id[8]; u8 name[1]; };
/* A player's pad record (MAT3 tPadState, gPlayers): 76 bytes, 80 on the PC. */
typedef struct Player { u8 unknown00[4];u32 edges;u8 unknown08[4];u32 autoEdges;u8 unknown10[56]; Profile **owner; } Player;
typedef struct FiveParts FiveParts;

extern Player gPlayers[];

VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view
VIEW(Profile **[], gSelectedPlayer);
#define gSelectedPlayer gSelectedPlayer_view

/* How many profile rows away from the selected one a row is. */
#define PROFILE_DISTANCE(x) ((x)>=0 ? (x) : -(x))

/* The select menu's rows (as playerscr.c names them). */
#define kSelectMenuJustPlay 0
#define kSelectMenuSelectPak 1
#define kSelectMenuPakStatus 2
#define kSelectMenuSelectPlayer 3       /* 3-10: the pak's profiles */
#define kSelectMenuLastPlayer 10
#define kSelectMenuCreatePlayer 11
#define kSelectMenuDeletePlayer 12
#define kSelectMenuCount 14
#define SELECT_MENU_PROFILES 8          /* the profile rows */

/* gSelectPlayerNum[]: a pak's profile count, or why it has none (playerscr.c's PAK_*). */
#define PAK_FULL (-5)                   /* never set: gPakFull says it */

#define NAME_GRID_LETTERS 54

/* gText's entries this file uses */
#define TEXT_PLAYER 29                  /* single[]: "player", in each window's title */
#define TEXT_CONTROLS_BUTTON 51         /* single[]: the controls button's label */
#define TEXT_SELECT_ROWS 21             /* tab[]: the select menu's rows' names */

/* 1 if profile `owner` is held by a player other than `skip`, else 0 (also for no profile). */
s32 playerClaimed(s32 skip,Profile **owner)
{
    s8 i;
    s32 found=0;
    if(owner)for(i=0;i<gNumPlayers;i++)if(i!=skip && owner==gPlayers[i].owner)found=1;
    return found;
}
/* playerBeingDeleted and playerBeingCreated take the player to skip as their second parameter after an unused one
 * (src/ovl_a/playerBeingDeleted.c: the N64 passed it in the second register); these calls give only the player. */
#define playerBeingDeleted(skip) playerBeingDeleted(0, skip)
#define playerBeingCreated(skip) playerBeingCreated(0, skip)

/* The select menu's text in player's window: the title ("player n") over the title button, a hint over the menu while
 * "just play" is selected, then each row's name beside its button (skipped while the button is edge-on, at its
 * alpha and depth, flashing when it faces front): the pak row with the pak's name, the pak's status line (why it
 * has no profiles, or full), each profile row the name of the pak's profile that many rows from the selected one
 * (a profile another player holds dark, a long name squeezed), create (dark while another player creates) and
 * delete; and the controls button's label beside the selected profile or "just play". */
void playerscr_drawSelect(s32 player)
{
    s32 draw_x,y,x;
    f32 scale=playerscr_gButtonScale;
    Profile *profile;
    u8 *label,text[32];
    s16 limit,point[2];
    s32 i,seen;
    f32 position[3];
    Profile **node;
    if(gNumPlayers>=2)scale*=0.5f;
    ChangePaneSize(playerscr_pane[player],-20,0,-10,10);
    if(gNumPlayers==1)HAL_SetFont(13);
    else if(gNumPlayers==2)HAL_SetFont(11);
    else HAL_SetFont(12);
    TB_Sprintf(text,playerscr_player_title_format,gText.single[TEXT_PLAYER],player+1);
    TB_SetTextColor(playerTextColor[player]);
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[2]-10.0f);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(text,x),y,text);
    HAL_SetFont(12);
    if(playerscr_gSelectMenuSelect[player]==kSelectMenuJustPlay) {
        position[2]=playerscr_buttonInfo[player][0].position[2];
        position[0]=0.0f;
        if(gNumPlayers==1)position[1]=130.0f;else position[1]=115.0f;
        ZOID_ProjectWindow(player,position,&gCamera[player],0,point);
        x=point[0];y=point[1];
        HAL_SetMenuTextDepth(position[2]-10.0f);
        TB_SetTextColor(kTB_WhiteText);
        label=gText.single[95];
        TB_SetTextColor(kTB_FlashText);
        TB_DrawText(TB_CenterText(label,x),y,label);
    }
    TB_GetFontLineHeight();
    if(gNumPlayers==1)HAL_SetFont(10);else HAL_SetFont(12);
    for(i=0;i<kSelectMenuCount;i++) {
        if(playerscr_buttonInfo[player][i].angle>EDGE_ON_FROM && playerscr_buttonInfo[player][i].angle<EDGE_ON_TO)continue;
        if(!playerscr_buttonInfo[player][i].alpha)continue;
        HAL_SetTextTranslucency((f32)(u32)playerscr_buttonInfo[player][i].alpha);
        if(playerscr_buttonInfo[player][i].angle<1.5707963705062866f) {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(kTB_WhiteText);
        } else {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(kTB_FlashText);
        }
        HAL_SetMenuTextDepth(playerscr_buttonInfo[player][i].position[2]-10.0f);
        ZOID_ProjectWindow(player,playerscr_buttonInfo[player][i].position,&gCamera[player],0,point);
        if(playerscr_buttonInfo[player][i].type==9)draw_x=point[0];
        else if(playerscr_buttonInfo[player][i].type==7)draw_x=point[0]-65.0f*scale*300.0f/playerscr_buttonZ;
        else draw_x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;
        y=point[1];
        if(i==1) {
            ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_PAK_VALUE].position,&gCamera[player],0,point);
            x=point[0]-25.0f*scale*300.0f/playerscr_buttonZ;
        }
        if(i<4)label=gText.taba[gText.tab[TEXT_SELECT_ROWS]+i];
        else if(i<11)label=gText.taba[gText.tab[TEXT_SELECT_ROWS]+kSelectMenuSelectPlayer];
        else label=gText.taba[gText.tab[TEXT_SELECT_ROWS]+i-7];
        if(i==0) {
            if(i==playerscr_gSelectMenuSelect[player])TB_SetTextColor(gSelectMenuControls[player]==0 ? kTB_FlashText : kTB_WhiteText);
            else TB_SetTextColor(kTB_WhiteText);
            TB_DrawText(draw_x,y,label);
        } else if(i==kSelectMenuSelectPak) {
            TB_SetTextColor(playerscr_gSelectMenuSelect[player]==kSelectMenuSelectPak ? kTB_FlashText : kTB_WhiteText);
            TB_DrawText(draw_x,y,label);
            label=gPS_PakName[playerscr_gSelectPakIdx[player]];
            TB_DrawText(x,y,label);
        } else if(i==kSelectMenuPakStatus) {
            switch(gSelectPlayerNum[player]) {
            case PAK_NO_CONTROLLER:label=gText.single[165];break;
            case PAK_RUMBLE:label=gText.single[169];break;
            case 0:label=gText.single[148];break;
            case PAK_DAMAGED:label=gText.single[166];break;
            case PAK_FULL:label=gText.single[161];break;
            case PAK_NOT_INSERTED:label=gText.single[168];break;
            }
            if(gPakFull[playerscr_gSelectPakIdx[player]])label=gText.single[161];
            TB_SetTextColor(0);
            TB_DrawText(TB_CenterText(label,draw_x),y,label);
        } else if(i>=kSelectMenuSelectPlayer && i<kSelectMenuCreatePlayer) {
            node=gSelectedPlayer[player];seen=0;
            if(node) {
                do {
                    profile=*node;
                    if(profile->fileh && (*profile->fileh)->pak==playerscr_gSelectPakIdx[player]) {
                        if(PROFILE_DISTANCE(i-playerscr_gSelectMenuSelect[player])==seen)break;
                        seen++;
                    }
                    if(i<playerscr_gSelectMenuSelect[player])node=profile->previous;else node=profile->next;
                } while(node);
            }
            if(!node) {
                if(playerscr_gSelectMenuSelect[player]<i || gSelectPlayerNum[player]<SELECT_MENU_PROFILES)node=gPlayerFiles.head;
                else node=gPlayerFiles.tail;
                while(node) {
                    profile=*node;
                    if(profile->fileh && (*profile->fileh)->pak==playerscr_gSelectPakIdx[player]) {
                        if(PROFILE_DISTANCE(i-((playerscr_gSelectMenuSelect[player]<i || gSelectPlayerNum[player]<SELECT_MENU_PROFILES) ? kSelectMenuSelectPlayer : kSelectMenuLastPlayer))==seen) {seen++;break;}
                        seen++;
                    }
                    if(playerscr_gSelectMenuSelect[player]<i || gSelectPlayerNum[player]<SELECT_MENU_PROFILES)node=profile->next;
                    else node=profile->previous;
                }
            }
            if(node) {
                profile=*node;
                label=profile->name;
                if(gNumPlayers==1)limit=98;
                else limit=(u16)(gNumPlayers==2 ? 70 : 55);
                if((u32)limit<(u32)HAL_GetTextWidth(label,-1))HAL_SetFontSpacing(0);
                if(i==playerscr_gSelectMenuSelect[player] && node==gSelectedPlayer[player])
                    TB_SetTextColor(gSelectMenuControls[player]==0 ? kTB_FlashText : kTB_WhiteText);
                else {
                    TB_SetTextColor(kTB_WhiteText);
                    if(playerClaimed(player,node))TB_SetTextColor(0);
                }
                TB_DrawEllipseText(draw_x,y,limit,label);
                HAL_SetFontSpacing(-1);
            }
        } else {
            TB_SetTextColor(i==playerscr_gSelectMenuSelect[player] ? kTB_FlashText : kTB_WhiteText);
            if((i==kSelectMenuCreatePlayer && playerBeingCreated(player)) || (i==kSelectMenuDeletePlayer && playerBeingDeleted(player)))TB_SetTextColor(0);
            TB_DrawText(draw_x,y,label);
        }
        TB_GetFontLineHeight();
    }
    if(playerscr_gSelectMenuSelect[player]==kSelectMenuJustPlay || (playerscr_gSelectMenuSelect[player]>=kSelectMenuSelectPlayer && playerscr_gSelectMenuSelect[player]<kSelectMenuCreatePlayer)) {
        ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].position,&gCamera[player],0,point);
        x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;y=point[1];
        TB_SetTextColor(gSelectMenuControls[player]==1 ? kTB_FlashText : kTB_WhiteText);
        if((playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].angle<=EDGE_ON_FROM || playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].angle>=EDGE_ON_TO) && playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].alpha) {
            HAL_SetTextTranslucency((f32)(u32)playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].alpha);
            if(playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].angle<1.5707963705062866f) {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(kTB_FlashText);
            }
            TB_DrawText(x,y,gText.single[TEXT_CONTROLS_BUTTON]);
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}
/* A waiting player's text: its profile's (or guest's) name over the title button, and in a box a "waiting for" line
 * with the one other player still choosing (in its colour), or a general line when several are. */
void drawWait(s32 player)
{
    s16 top,width;
    s32 x,y;
    s16 point[2];
    s32 i,count,other;
    u8 *text;
    if(gNumPlayers>=2)HAL_SetFont(11);else HAL_SetFont(13);
    if(gPlayers[player].owner)text=(*gPlayers[player].owner)->name;
    else text=gJustPlayers[player]->name;
    TB_SetTextColor(playerTextColor[player]);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[2]-10.0f);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawEllipseText(x,y,69,text);
    y+=TB_GetFontLineHeight()*2;
    top=y-4;
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetMenuTextDepth(0.0f);
    text=gText.single[151];
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(text,x),y,text);
    width=HAL_GetTextWidth(text,-1);
    y+=TB_GetFontLineHeight();
    count=0;
    for(i=0;i<gNumPlayers;i++) {
        if(i!=player && gPlayerState[i]!=kPlayerStateWait) {other=i;count++;}
    }
    if(count==1) {
        TB_SetTextColor(playerTextColor[other]);
        if(gPlayers[other].owner)text=(*gPlayers[other].owner)->name;
        else text=gJustPlayers[other]->name;
    } else text=gText.single[31];
    TB_DrawText(TB_CenterText(text,x),y,text);
    if((u32)width<(u32)HAL_GetTextWidth(text,-1))width=HAL_GetTextWidth(text,-1);
    x=playerscr_gPlayerRects[gNumPlayers-1][player].cx;
    ChangePaneSize(playerscr_pane[player],x-width/2-4,top,(x+4)+width/2,top+TB_GetFontLineHeight()*2+8);
}
/* A new profile's name entry: the name so far (flashing) over the title button, a heading with one window, the
 * 6 x 9 letter grid (the selected one flashing) with space, delete and end under it, all in a box. */
void drawCreatePlayer(s32 player)
{
    s32 i;
    u8 text[2];
    s32 x,y,columns,width;
    s16 point[2],top;
    if(gNumPlayers>=2)HAL_SetFont(11);else HAL_SetFont(13);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[2]-10.0f);
    TB_SetTextColor(kTB_FlashText);
    if(gNumPlayers==1U)width=150;else width=69;
    TB_DrawEllipseText(x,y,width,gCreatePlayerNames[player]);
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    HAL_SetFont(11);
    HAL_SetMenuTextDepth(0.0f);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    text[1]=0;
    x=playerscr_gPlayerRects[gNumPlayers-1][player].cx;
    y=playerscr_gPlayerRects[gNumPlayers-1][player].y+32;
    if(gNumPlayers>=2)y-=26;
    if(gNumPlayers==1U) {
        top=y-4;
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawText(TB_CenterText(gText.single[150],x),y,gText.single[150]);
    }
    HAL_SetFont(12);
    x=playerscr_gPlayerRects[gNumPlayers-1][player].cx-48;
    y=playerscr_gPlayerRects[gNumPlayers-1][player].y+46;
    if(gNumPlayers>=2) {y-=26;top=y-4;}
    columns=NAME_GRID_COLUMNS;
    for(i=0;i<NAME_GRID_LETTERS;i++) {
        text[0]=gPlayerNameChars[i];
        TB_SetTextColor(i==playerscr_gNameSelect[player] ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(TB_CenterText(text,x),y,text);
        if((i+1)%columns==0) {x=playerscr_gPlayerRects[gNumPlayers-1][player].cx-48;y+=10;}
        else x+=12;
    }
    x=playerscr_gPlayerRects[gNumPlayers-1][player].cx-36;
    TB_SetTextColor(playerscr_gNameSelect[player]==NAME_SEL_SPACE ? 22 : 16);
    TB_DrawText(TB_CenterText(playerscr_spc,x),y,playerscr_spc);
    x+=36;
    TB_SetTextColor(playerscr_gNameSelect[player]==NAME_SEL_DELETE ? 22 : 16);
    TB_DrawText(TB_CenterText(playerscr_del,x),y,playerscr_del);
    x+=36;
    TB_SetTextColor(playerscr_gNameSelect[player]==NAME_SEL_END ? 22 : 16);
    TB_DrawText(TB_CenterText(playerscr_end,x),y,playerscr_end);
    y+=10;
    ChangePaneSize(playerscr_pane[player],playerscr_gPlayerRects[gNumPlayers-1][player].cx-56,top,playerscr_gPlayerRects[gNumPlayers-1][player].cx+56,y+4);
}

/* The delete menu's text: the title; then the "are you sure" question about the profile (yes / no, the answer
 * flashing), or the "deleted" notice, each in a box; or the menu: the pak row with the pak's name, the pak's status,
 * and the pak's profiles (one another player holds dark, one that can't be deleted grey). */
void drawDeletePlayer(s32 player)
{
    f32 scale=playerscr_gButtonScale;
    s32 y,x;
    u8 *label,text[128];
    s32 draw_x,i,seen;
    s16 point[2],top,lines,limit;
    s8 selected;
    f32 delta;
    Profile **node;
    if(gNumPlayers>=2)scale*=0.5f;
    if(gNumPlayers>=2)HAL_SetFont(12);else HAL_SetFont(13);
    label=gText.single[140];
    TB_SetTextColor(playerTextColor[player]);
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[2]-10.0f);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(label,x),y,label);
    y+=TB_GetFontLineHeight();
    if(gNumPlayers>=2)HAL_SetFont(12);else HAL_SetFont(10);
    if(gDeleteMenuConfirm[player]==1) {
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        HAL_SetMenuTextDepth(0.0f);
        y+=TB_GetFontLineHeight();
        top=y-4;
        TB_Sprintf(text,playerscr_delete_message_format,gText.single[141],gDeleteMenuName[player],gText.single[142]);
        TB_SetTextColor(kTB_WhiteText);
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(x,y,playerscr_gPlayerRects[gNumPlayers-1][player].w,
                           playerscr_gPlayerRects[gNumPlayers-1][player].y+playerscr_gPlayerRects[gNumPlayers-1][player].h-y,-1,0,text);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        lines=TB_GetTextNumLines(playerscr_gPlayerRects[gNumPlayers-1][player].w,text);
        y+=TB_GetFontLineHeight()*(lines+1);
        label=gText.taba[gText.tab[TEXT_YES_NO]];
        TB_SetTextColor(gDeleteMenuYes[player]==1 ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(TB_RightText(label,x-5),y,label);
        label=gText.taba[gText.tab[TEXT_YES_NO]+1];
        TB_SetTextColor(gDeleteMenuYes[player]==0 ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(x+5,y,label);
        y+=TB_GetFontLineHeight();
        ChangePaneSize(playerscr_pane[player],x-playerscr_gPlayerRects[gNumPlayers-1][player].w/2-4,top,
                         x+playerscr_gPlayerRects[gNumPlayers-1][player].w/2+4,y+4);
    } else if(gDeleteMenuNotify[player]==1) {
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        HAL_SetMenuTextDepth(0.0f);
        y+=TB_GetFontLineHeight();
        top=y-4;
        TB_Sprintf(text,gText.single[145],gDeleteMenuName[player]);
        TB_SetTextColor(kTB_WhiteText);
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(x,y,playerscr_gPlayerRects[gNumPlayers-1][player].w,
                           playerscr_gPlayerRects[gNumPlayers-1][player].y+playerscr_gPlayerRects[gNumPlayers-1][player].h-y,-1,0,text);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        lines=TB_GetTextNumLines(playerscr_gPlayerRects[gNumPlayers-1][player].w,text);
        y+=TB_GetFontLineHeight()*lines;
        ChangePaneSize(playerscr_pane[player],x-playerscr_gPlayerRects[gNumPlayers-1][player].w/2-4,top,
                         x+playerscr_gPlayerRects[gNumPlayers-1][player].w/2+4,y+4);
    } else {
        ChangePaneSize(playerscr_pane[player],-20,0,-10,10);
        for(i=0;i<kSelectMenuCount;i++) {
            if(playerscr_buttonInfo[player][i].angle>EDGE_ON_FROM && playerscr_buttonInfo[player][i].angle<EDGE_ON_TO)continue;
            if(!playerscr_buttonInfo[player][i].alpha)continue;
            HAL_SetTextTranslucency((f32)(u32)playerscr_buttonInfo[player][i].alpha);
            if(playerscr_buttonInfo[player][i].angle<1.5707963705062866f) {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(kTB_FlashText);
            }
            HAL_SetMenuTextDepth(playerscr_buttonInfo[player][i].position[2]-10.0f);
            ZOID_ProjectWindow(player,playerscr_buttonInfo[player][i].position,&gCamera[player],0,point);
            if(playerscr_buttonInfo[player][i].type==9)draw_x=point[0]-105.0f*scale*300.0f/playerscr_buttonZ;
            else if(playerscr_buttonInfo[player][i].type==7)draw_x=point[0]-65.0f*scale*300.0f/playerscr_buttonZ;
            else draw_x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;
            y=point[1];
            if(i==kSelectMenuJustPlay || (i>=kSelectMenuSelectPlayer && i<kSelectMenuCreatePlayer)) {
                ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_CONTROLS].position,&gCamera[player],0,point);
                x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;
            } else if(i==kSelectMenuSelectPak) {
                ZOID_ProjectWindow(player,playerscr_buttonInfo[player][PLAYER_BUTTON_PAK_VALUE].position,&gCamera[player],0,point);
                x=point[0]-25.0f*scale*300.0f/playerscr_buttonZ;
            }
            if(i==kSelectMenuSelectPak) {
                TB_SetTextColor(gDeleteMenuSelect[player]==kSelectMenuSelectPak ? kTB_FlashText : kTB_WhiteText);
                label=gText.taba[gText.tab[TEXT_SELECT_ROWS]+i];
                TB_DrawText(draw_x,y,label);
                label=gPS_PakName[playerscr_gSelectPakIdx[player]];
                TB_DrawText(x,y,label);
            } else if(i==kSelectMenuPakStatus) {
                delta=65.f*scale*300.0f/playerscr_buttonZ;
                switch(gSelectPlayerNum[player]) {
                case PAK_NO_CONTROLLER:label=gText.single[165];break;
                case PAK_RUMBLE:label=gText.single[169];break;
                case 0:label=gText.single[148];break;
                case PAK_DAMAGED:label=gText.single[166];break;
                case PAK_FULL:label=gText.single[161];break;
                case PAK_NOT_INSERTED:label=gText.single[168];break;
                }
                draw_x+=delta;
                TB_SetTextColor(0);
                TB_DrawText(TB_CenterText(label,draw_x),y,label);
            } else if(i>=kSelectMenuSelectPlayer && i<kSelectMenuCreatePlayer) {
                node=gSelectedPlayer[player];seen=0;
                if(node) {
                    selected=gDeleteMenuSelect[player];
                    do {
                        if((*node)->fileh && (*(*node)->fileh)->pak==playerscr_gSelectPakIdx[player]) {
                            if(PROFILE_DISTANCE(i-selected)==seen)break;
                            seen++;
                        }
                        if(i<selected)node=(*node)->previous;else node=(*node)->next;
                    } while(node);
                }
                if(!node) {
                    selected=gDeleteMenuSelect[player];
                    if(selected<i || gSelectPlayerNum[player]<SELECT_MENU_PROFILES)node=gPlayerFiles.head;
                    else node=gPlayerFiles.tail;
                    while(node) {
                        if((*node)->fileh && (*(*node)->fileh)->pak==playerscr_gSelectPakIdx[player]) {
                            if(PROFILE_DISTANCE(i-((selected<i || gSelectPlayerNum[player]<SELECT_MENU_PROFILES) ? kSelectMenuSelectPlayer : kSelectMenuLastPlayer))==seen) {seen++;break;}
                            seen++;
                        }
                        if(selected<i || gSelectPlayerNum[player]<SELECT_MENU_PROFILES)node=(*node)->next;
                        else node=(*node)->previous;
                    }
                }
                if(node) {
                    label=(*node)->name;
                    if(gNumPlayers==1)limit=98;
                    else limit=(u16)(gNumPlayers==2 ? 70 : 55);
                    if((u32)limit<(u32)HAL_GetTextWidth(label,-1))HAL_SetFontSpacing(0);
                    if(i==gDeleteMenuSelect[player] && node==gSelectedPlayer[player])TB_SetTextColor(kTB_FlashText);
                    else {
                        TB_SetTextColor(kTB_WhiteText);
                        if(playerClaimed(player,node))TB_SetTextColor(0);
                        if(!playerscr_gSelectMenuValid[player][i])TB_SetTextColor(4);
                    }
                    TB_DrawEllipseText(draw_x,y,limit,label);
                    HAL_SetFontSpacing(-1);
                }
            }
            TB_GetFontLineHeight();
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}

/* The screen's text blit callback: each window's text by its state (the controls state draws nothing, the controls
 * screen has the window; a full or damaged pak shows its notice under the window's title). Always 1. */
s32 playerscr_animText(void *callback_context)
{
    s32 player,x,y;
    HAL_SetTextDepth(0.0f);
    for(player=0;player<gNumPlayers;player++) {
        switch(gPlayerState[player]) {
        case kPlayerStateSelect:playerscr_drawSelect(player);break;
        case kPlayerStateWait:drawWait(player);break;
        case kPlayerStateControls:ChangePaneSize(playerscr_pane[player],-20,0,-10,10);break;
        case kPlayerStateCreatePlayer:drawCreatePlayer(player);break;
        case kPlayerStateDeletePlayer:drawDeletePlayer(player);break;
        case kPlayerStatePakFull: {
            u8 text[64];
            y=playerscr_gPlayerRects[gNumPlayers-1][player].y;
            x=playerscr_gPlayerRects[gNumPlayers-1][player].cx;
            if(gNumPlayers>=3)HAL_SetFont(11);
            else if(gNumPlayers==2)HAL_SetFont(10);
            else HAL_SetFont(13);
            TB_Sprintf(text,playerscr_pak_full_title_format,gText.single[TEXT_PLAYER],player+1);
            TB_SetTextColor(playerTextColor[player]);
            TB_DrawText(TB_CenterText(text,x),y,text);
            y+=TB_GetFontLineHeight()*2;
            if(gNumPlayers>=3)HAL_SetFont(12);
            else if(gNumPlayers==2)HAL_SetFont(11);
            else HAL_SetFont(10);
            TB_SetTextColor(17);
            TB_DrawText(TB_CenterText(playerscr_pak_full_text,x),y,playerscr_pak_full_text);
            break;
        }
        case kPlayerStatePakDamaged: {
            u8 text[64];
            y=playerscr_gPlayerRects[gNumPlayers-1][player].y;
            x=playerscr_gPlayerRects[gNumPlayers-1][player].cx;
            if(gNumPlayers>=3)HAL_SetFont(11);
            else if(gNumPlayers==2)HAL_SetFont(10);
            else HAL_SetFont(13);
            TB_Sprintf(text,playerscr_pak_damaged_title_format,gText.single[TEXT_PLAYER],player+1);
            TB_SetTextColor(playerTextColor[player]);
            TB_DrawText(TB_CenterText(text,x),y,text);
            y+=TB_GetFontLineHeight()*2;
            if(gNumPlayers>=3)HAL_SetFont(12);
            else if(gNumPlayers==2)HAL_SetFont(11);
            else HAL_SetFont(10);
            TB_SetTextColor(17);
            TB_DrawText(TB_CenterText(playerscr_pak_damaged_text,x),y,playerscr_pak_damaged_text);
            break;
        }
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}

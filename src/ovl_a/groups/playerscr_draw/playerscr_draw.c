/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image-A profile-selection renderer A7B58, complete native3196-byte extent.
 * Actual96FA0 controller supplies eligibility-helper call frequency; the two
 * one-formal helpers use the stable player-count bound, preserving their
 * complete92-byte native bodies and genuine skip=a1 private convention.
 * Indexed slots, guarded walks, split seen postincrement and direct selection
 * reads recover native control/data lifetimes. A real profile pointer is
 * reused across traversal and label selection. The u16 width-choice boundary
 * and one compiled-out label read are explicit source shaping: the latter
 * raises the traced name-pointer allocation priority above width. Neither
 * changes the consumed values or emits an extra native instruction.
 * The one temporary text writer is bounded by native format and all six
 * supported languages:10 bytes including NUL for player0..3,20 for the
 * formatter's independent signed32 domain.32-byte storage plus native home
 * order is a safe reconstruction; exact original capacity remains UNKNOWN.
 * No forced compiler, false formal, padding or unused scalar is introduced.
 * All2508 inherited bytes are exact. A9088/A9EEC/96FA0 remain NONMATCH context.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Language { s32 unknown0; u8 **text; s32 unknown8; u16 *indices; u8 **indexed; } Language;
typedef struct Color4 { u32 word; } Color4;
typedef struct Slot { s32 kind,handle; f32 angle,matrix[3][3],position[3]; u8 alpha,unknown3D[3]; } Slot;
typedef struct Camera { f32 uv[3][3],position[3]; u8 unknown30[104]; } Camera;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct FileInfo { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 controller; } FileInfo;
typedef struct Profile Profile;
struct Profile { Profile **next,**previous; FileInfo **file; u8 unknown0C[8]; u8 name[1]; };
typedef struct Player { u8 unknown00[4];u32 pressed;u8 unknown08[4];u32 repeat;u8 unknown10[56]; Profile **owner; } Player;
typedef struct Layout { s32 unknown0,y,width,height,x,unknown14; } Layout;
typedef struct FiveParts FiveParts;
extern Language gText;
extern Slot playerscr_buttonInfo[][22];
extern Camera gCamera[];
extern Layout playerscr_gPlayerRects[][4];
extern f32 playerscr_gButtonScale,playerscr_buttonZ;
extern s16 gNumPlayers;
extern Player gPlayers[];
extern Profile *gJustPlayers[];
extern s32 playerTextColor[];
extern Color4 flashColors,flashColors_at_1;
extern Profile **gPlayerFiles_head,**gPlayerFiles_tail,**gSelectedPlayer[];
extern void *gPakFull[];
extern FiveParts *playerscr_pane[];
extern s32 gPlayerState[],playerscr_gSelectMenuSelect[];
extern s8 gSelectMenuControls[],playerscr_gSelectPakIdx[],gSelectPlayerNum[],gDeleteMenuSelect[],gDeleteMenuConfirm[],gDeleteMenuYes[],gDeleteMenuNotify[];
extern s8 playerscr_gSelectMenuValid[][14];
extern u8 gCreatePlayerNames[][13],gDeleteMenuName[][13];
extern s16 playerscr_gNameSelect[];
extern u8 *gPlayerNameChars,*gPS_PakName[];
extern u8 playerscr_del[],playerscr_end[],playerscr_spc[];
extern u8 playerscr_player_title_format[],playerscr_delete_message_format[],playerscr_pak_full_title_format[],playerscr_pak_full_text[],playerscr_pak_damaged_title_format[],playerscr_pak_damaged_text[];
extern void HAL_SetTextDepth(f32);
extern s32 HAL_SetFont(s32);
extern void TB_SetTextColor(s32);
extern void TB_SetJust(u32,u32);
extern void ZOID_ProjectWindow(s32,f32 *,Camera *,void *,s16 *);
extern s16 TB_CenterText(void *,s16);
extern s16 TB_RightText(void *,s16);
extern s16 TB_GetFontLineHeight(void);
extern s32 HAL_GetTextWidth(u8 *,s16);
extern void TB_DrawText(s16,s16,void *);
extern void TB_Sprintf(u8 *,const u8 *,...);
extern void HAL_SetTextTranslucency(f32);
extern void TB_FlashTextColors(Color4,Color4);
extern void HAL_SetMenuTextDepth(f32);
extern void HAL_SetFontSpacing(s32);
extern void TB_DrawTextScrollBox(s16,s16,s16,s16,s16,s16,u8 *);
extern s16 TB_GetTextNumLines(s16,u8 *);
extern void TB_DrawEllipseText(s16,s16,s16,void *);
extern void ChangePaneSize(FiveParts *,s32,s32,s32,s32);
/* The native integer absolute-value expression repeats its operand. */
#define ABS(x) ((x)<0 ? -(x) : (x))
#define PROFILE_DISTANCE(x) ((x)>=0 ? (x) : -(x))

/* The native owner helper checks for a null handle before reading count.
 * Directly using the stable player-count bound removes its eager snapshot
 * and lets the actual index and bound receive their native IPA registers.
 * owner is passed in a0 and skip in a2 by both real renderer children.
 * The two sibling eligibility helpers are preserved exact members. */
s32 playerClaimed(s32 skip,Profile **owner)
{
    s8 i;
    s32 found=0;
    if(owner)for(i=0;i<gNumPlayers;i++)if(i!=skip && owner==gPlayers[i].owner)found=1;
    return found;
}
/* playerBeingDeleted/playerBeingCreated (playerBeingDeleted/Created) take `skip` in a1 under IDO's IPA: their owners
 * (src/ovl_a/playerBeingDeleted.c, playerBeingCreated.c) read it as a second parameter after a dummy, and these calls
 * pass it alone. On the PC the dummy is explicit (docs/port/PROTOTYPES.md). */
#define PC_IPA_A0 s32 pc_a0,
s32 playerBeingDeleted(PC_IPA_A0 s32 skip);
s32 playerBeingCreated(PC_IPA_A0 s32 skip);
s32 playerClaimed(s32,Profile **);
s32 playerBeingCreated(PC_IPA_A0 s32);
s32 playerBeingDeleted(PC_IPA_A0 s32);
#define playerBeingDeleted(skip) playerBeingDeleted(0, skip)
#define playerBeingCreated(skip) playerBeingCreated(0, skip)
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
    TB_Sprintf(text,playerscr_player_title_format,gText.text[29],player+1);
    TB_SetTextColor(playerTextColor[player]);
    TB_SetJust(0,1);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][16].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][16].position[2]-10.0f);
    TB_SetTextColor(1);
    TB_DrawText(TB_CenterText(text,x),y,text);
    HAL_SetFont(12);
    if(playerscr_gSelectMenuSelect[player]==0) {
        position[2]=playerscr_buttonInfo[player][0].position[2];
        position[0]=0.0f;
        if(gNumPlayers==1)position[1]=130.0f;else position[1]=115.0f;
        ZOID_ProjectWindow(player,position,&gCamera[player],0,point);
        x=point[0];y=point[1];
        HAL_SetMenuTextDepth(position[2]-10.0f);
        TB_SetTextColor(1);
        label=gText.text[95];
        TB_SetTextColor(22);
        TB_DrawText(TB_CenterText(label,x),y,label);
    }
    TB_GetFontLineHeight();
    if(gNumPlayers==1)HAL_SetFont(10);else HAL_SetFont(12);
    for(i=0;i<14;i++) {
        if(playerscr_buttonInfo[player][i].angle>0.6981316804885864f && playerscr_buttonInfo[player][i].angle<2.4434609413146973f)continue;
        if(!playerscr_buttonInfo[player][i].alpha)continue;
        HAL_SetTextTranslucency((f32)(u32)playerscr_buttonInfo[player][i].alpha);
        if(playerscr_buttonInfo[player][i].angle<1.5707963705062866f) {
            TB_FlashTextColors(flashColors,flashColors_at_1);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors(flashColors,flashColors_at_1);
            TB_SetTextColor(22);
        }
        HAL_SetMenuTextDepth(playerscr_buttonInfo[player][i].position[2]-10.0f);
        ZOID_ProjectWindow(player,playerscr_buttonInfo[player][i].position,&gCamera[player],0,point);
        if(playerscr_buttonInfo[player][i].kind==9)draw_x=point[0];
        else if(playerscr_buttonInfo[player][i].kind==7)draw_x=point[0]-65.0f*scale*300.0f/playerscr_buttonZ;
        else draw_x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;
        y=point[1];
        if(i==1) {
            ZOID_ProjectWindow(player,playerscr_buttonInfo[player][15].position,&gCamera[player],0,point);
            x=point[0]-25.0f*scale*300.0f/playerscr_buttonZ;
        }
        if(i<4)label=gText.indexed[gText.indices[21]+i];
        else if(i<11)label=gText.indexed[gText.indices[21]+3];
        else label=gText.indexed[gText.indices[21]+i-7];
        if(i==0) {
            if(i==playerscr_gSelectMenuSelect[player])TB_SetTextColor(gSelectMenuControls[player]==0 ? 22 : 1);
            else TB_SetTextColor(1);
            TB_DrawText(draw_x,y,label);
        } else if(i==1) {
            TB_SetTextColor(playerscr_gSelectMenuSelect[player]==1 ? 22 : 1);
            TB_DrawText(draw_x,y,label);
            label=gPS_PakName[playerscr_gSelectPakIdx[player]];
            TB_DrawText(x,y,label);
        } else if(i==2) {
            switch(gSelectPlayerNum[player]) {
            case -4:label=gText.text[165];break;
            case -3:label=gText.text[169];break;
            case 0:label=gText.text[148];break;
            case -2:label=gText.text[166];break;
            case -5:label=gText.text[161];break;
            case -1:label=gText.text[168];break;
            }
            if(gPakFull[playerscr_gSelectPakIdx[player]])label=gText.text[161];
            TB_SetTextColor(0);
            TB_DrawText(TB_CenterText(label,draw_x),y,label);
        } else if(i>=3 && i<11) {
            node=gSelectedPlayer[player];seen=0;
            if(node) {
                do {
                    profile=*node;
                    if(profile->file && (*profile->file)->controller==playerscr_gSelectPakIdx[player]) {
                        if(PROFILE_DISTANCE(i-playerscr_gSelectMenuSelect[player])==seen)break;
                        seen++;
                    }
                    if(i<playerscr_gSelectMenuSelect[player])node=profile->previous;else node=profile->next;
                } while(node);
            }
            if(!node) {
                if(playerscr_gSelectMenuSelect[player]<i || gSelectPlayerNum[player]<8)node=gPlayerFiles_head;
                else node=gPlayerFiles_tail;
                while(node) {
                    profile=*node;
                    if(profile->file && (*profile->file)->controller==playerscr_gSelectPakIdx[player]) {
                        if(PROFILE_DISTANCE(i-((playerscr_gSelectMenuSelect[player]<i || gSelectPlayerNum[player]<8) ? 3 : 10))==seen) {seen++;break;}
                        seen++;
                    }
                    if(playerscr_gSelectMenuSelect[player]<i || gSelectPlayerNum[player]<8)node=profile->next;
                    else node=profile->previous;
                }
            }
            if(node) {
                profile=*node;
                label=profile->name;
                if(gNumPlayers==1)limit=98;
                else limit=(u16)(gNumPlayers==2 ? 70 : 55);
                if((u32)limit<(u32)HAL_GetTextWidth(label,-1))HAL_SetFontSpacing(0);
                if(label) {}
                if(i==playerscr_gSelectMenuSelect[player] && node==gSelectedPlayer[player])
                    TB_SetTextColor(gSelectMenuControls[player]==0 ? 22 : 1);
                else {
                    TB_SetTextColor(1);
                    if(playerClaimed(player,node))TB_SetTextColor(0);
                }
                TB_DrawEllipseText(draw_x,y,limit,label);
                HAL_SetFontSpacing(-1);
            }
        } else {
            TB_SetTextColor(i==playerscr_gSelectMenuSelect[player] ? 22 : 1);
            if((i==11 && playerBeingCreated(player)) || (i==12 && playerBeingDeleted(player)))TB_SetTextColor(0);
            TB_DrawText(draw_x,y,label);
        }
        TB_GetFontLineHeight();
    }
    if(playerscr_gSelectMenuSelect[player]==0 || (playerscr_gSelectMenuSelect[player]>=3 && playerscr_gSelectMenuSelect[player]<11)) {
        ZOID_ProjectWindow(player,playerscr_buttonInfo[player][14].position,&gCamera[player],0,point);
        x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;y=point[1];
        TB_SetTextColor(gSelectMenuControls[player]==1 ? 22 : 1);
        if((playerscr_buttonInfo[player][14].angle<=0.6981316804885864f || playerscr_buttonInfo[player][14].angle>=2.4434609413146973f) && playerscr_buttonInfo[player][14].alpha) {
            HAL_SetTextTranslucency((f32)(u32)playerscr_buttonInfo[player][14].alpha);
            if(playerscr_buttonInfo[player][14].angle<1.5707963705062866f) {
                TB_FlashTextColors(flashColors,flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors,flashColors_at_1);
                TB_SetTextColor(22);
            }
            TB_DrawText(x,y,gText.text[51]);
        }
    }
    TB_SetJust(0,3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}
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
    TB_SetJust(1,1);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][16].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][16].position[2]-10.0f);
    TB_SetTextColor(1);
    TB_DrawEllipseText(x,y,69,text);
    y+=TB_GetFontLineHeight()*2;
    top=y-4;
    TB_SetJust(0,3);
    HAL_SetMenuTextDepth(0.0f);
    text=gText.text[151];
    TB_SetTextColor(1);
    TB_DrawText(TB_CenterText(text,x),y,text);
    width=HAL_GetTextWidth(text,-1);
    y+=TB_GetFontLineHeight();
    count=0;
    for(i=0;i<gNumPlayers;i++) {
        if(i!=player && gPlayerState[i]!=1) {other=i;count++;}
    }
    if(count==1) {
        TB_SetTextColor(playerTextColor[other]);
        if(gPlayers[other].owner)text=(*gPlayers[other].owner)->name;
        else text=gJustPlayers[other]->name;
    } else text=gText.text[31];
    TB_DrawText(TB_CenterText(text,x),y,text);
    if((u32)width<(u32)HAL_GetTextWidth(text,-1))width=HAL_GetTextWidth(text,-1);
    x=playerscr_gPlayerRects[gNumPlayers-1][player].x;
    ChangePaneSize(playerscr_pane[player],x-width/2-4,top,(x+4)+width/2,top+TB_GetFontLineHeight()*2+8);
}
void drawCreatePlayer(s32 player)
{
    s32 i;
    u8 text[2];
    s32 x,y,columns,width;
    s16 point[2],top;
    if(gNumPlayers>=2)HAL_SetFont(11);else HAL_SetFont(13);
    TB_SetJust(1,1);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][16].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][16].position[2]-10.0f);
    TB_SetTextColor(22);
    if(gNumPlayers==1U)width=150;else width=69;
    TB_DrawEllipseText(x,y,width,gCreatePlayerNames[player]);
    TB_SetJust(0,1);
    HAL_SetFont(11);
    HAL_SetMenuTextDepth(0.0f);
    TB_SetJust(0,3);
    text[1]=0;
    x=playerscr_gPlayerRects[gNumPlayers-1][player].x;
    y=playerscr_gPlayerRects[gNumPlayers-1][player].y+32;
    if(gNumPlayers>=2)y-=26;
    if(gNumPlayers==1U) {
        top=y-4;
        TB_SetTextColor(1);
        TB_DrawText(TB_CenterText(gText.text[150],x),y,gText.text[150]);
    }
    HAL_SetFont(12);
    x=playerscr_gPlayerRects[gNumPlayers-1][player].x-48;
    y=playerscr_gPlayerRects[gNumPlayers-1][player].y+46;
    if(gNumPlayers>=2) {y-=26;top=y-4;}
    columns=9;
    for(i=0;i<54;i++) {
        text[0]=gPlayerNameChars[i];
        TB_SetTextColor(i==playerscr_gNameSelect[player] ? 22 : 1);
        TB_DrawText(TB_CenterText(text,x),y,text);
        if((i+1)%columns==0) {x=playerscr_gPlayerRects[gNumPlayers-1][player].x-48;y+=10;}
        else x+=12;
    }
    x=playerscr_gPlayerRects[gNumPlayers-1][player].x-36;
    TB_SetTextColor(playerscr_gNameSelect[player]==-3 ? 22 : 16);
    TB_DrawText(TB_CenterText(playerscr_spc,x),y,playerscr_spc);
    x+=36;
    TB_SetTextColor(playerscr_gNameSelect[player]==-2 ? 22 : 16);
    TB_DrawText(TB_CenterText(playerscr_del,x),y,playerscr_del);
    x+=36;
    TB_SetTextColor(playerscr_gNameSelect[player]==-1 ? 22 : 16);
    TB_DrawText(TB_CenterText(playerscr_end,x),y,playerscr_end);
    y+=10;
    ChangePaneSize(playerscr_pane[player],playerscr_gPlayerRects[gNumPlayers-1][player].x-56,top,playerscr_gPlayerRects[gNumPlayers-1][player].x+56,y+4);
}
s32 playerClaimed(s32,Profile **);
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
    label=gText.text[140];
    TB_SetTextColor(playerTextColor[player]);
    TB_SetJust(0,1);
    ZOID_ProjectWindow(player,playerscr_buttonInfo[player][16].position,&gCamera[player],0,point);
    x=point[0];y=point[1];
    HAL_SetMenuTextDepth(playerscr_buttonInfo[player][16].position[2]-10.0f);
    TB_SetTextColor(1);
    TB_DrawText(TB_CenterText(label,x),y,label);
    y+=TB_GetFontLineHeight();
    if(gNumPlayers>=2)HAL_SetFont(12);else HAL_SetFont(10);
    if(gDeleteMenuConfirm[player]==1) {
        TB_SetJust(0,3);
        HAL_SetMenuTextDepth(0.0f);
        y+=TB_GetFontLineHeight();
        top=y-4;
        TB_Sprintf(text,playerscr_delete_message_format,gText.text[141],gDeleteMenuName[player],gText.text[142]);
        TB_SetTextColor(1);
        TB_SetJust(1,3);
        TB_DrawTextScrollBox(x,y,playerscr_gPlayerRects[gNumPlayers-1][player].width,
                           playerscr_gPlayerRects[gNumPlayers-1][player].y+playerscr_gPlayerRects[gNumPlayers-1][player].height-y,-1,0,text);
        TB_SetJust(0,3);
        lines=TB_GetTextNumLines(playerscr_gPlayerRects[gNumPlayers-1][player].width,text);
        y+=TB_GetFontLineHeight()*(lines+1);
        label=gText.indexed[gText.indices[24]];
        TB_SetTextColor(gDeleteMenuYes[player]==1 ? 22 : 1);
        TB_DrawText(TB_RightText(label,x-5),y,label);
        label=gText.indexed[gText.indices[24]+1];
        TB_SetTextColor(gDeleteMenuYes[player]==0 ? 22 : 1);
        TB_DrawText(x+5,y,label);
        y+=TB_GetFontLineHeight();
        ChangePaneSize(playerscr_pane[player],x-playerscr_gPlayerRects[gNumPlayers-1][player].width/2-4,top,
                         x+playerscr_gPlayerRects[gNumPlayers-1][player].width/2+4,y+4);
    } else if(gDeleteMenuNotify[player]==1) {
        TB_SetJust(0,3);
        HAL_SetMenuTextDepth(0.0f);
        y+=TB_GetFontLineHeight();
        top=y-4;
        TB_Sprintf(text,gText.text[145],gDeleteMenuName[player]);
        TB_SetTextColor(1);
        TB_SetJust(1,3);
        TB_DrawTextScrollBox(x,y,playerscr_gPlayerRects[gNumPlayers-1][player].width,
                           playerscr_gPlayerRects[gNumPlayers-1][player].y+playerscr_gPlayerRects[gNumPlayers-1][player].height-y,-1,0,text);
        TB_SetJust(0,3);
        lines=TB_GetTextNumLines(playerscr_gPlayerRects[gNumPlayers-1][player].width,text);
        y+=TB_GetFontLineHeight()*lines;
        ChangePaneSize(playerscr_pane[player],x-playerscr_gPlayerRects[gNumPlayers-1][player].width/2-4,top,
                         x+playerscr_gPlayerRects[gNumPlayers-1][player].width/2+4,y+4);
    } else {
        ChangePaneSize(playerscr_pane[player],-20,0,-10,10);
        for(i=0;i<14;i++) {
            if(playerscr_buttonInfo[player][i].angle>0.6981316804885864f && playerscr_buttonInfo[player][i].angle<2.4434609413146973f)continue;
            if(!playerscr_buttonInfo[player][i].alpha)continue;
            HAL_SetTextTranslucency((f32)(u32)playerscr_buttonInfo[player][i].alpha);
            if(playerscr_buttonInfo[player][i].angle<1.5707963705062866f) {
                TB_FlashTextColors(flashColors,flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors,flashColors_at_1);
                TB_SetTextColor(22);
            }
            HAL_SetMenuTextDepth(playerscr_buttonInfo[player][i].position[2]-10.0f);
            ZOID_ProjectWindow(player,playerscr_buttonInfo[player][i].position,&gCamera[player],0,point);
            if(playerscr_buttonInfo[player][i].kind==9)draw_x=point[0]-105.0f*scale*300.0f/playerscr_buttonZ;
            else if(playerscr_buttonInfo[player][i].kind==7)draw_x=point[0]-65.0f*scale*300.0f/playerscr_buttonZ;
            else draw_x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;
            y=point[1];
            if(i==0 || (i>=3 && i<11)) {
                ZOID_ProjectWindow(player,playerscr_buttonInfo[player][14].position,&gCamera[player],0,point);
                x=point[0]-45.0f*scale*300.0f/playerscr_buttonZ;
            } else if(i==1) {
                ZOID_ProjectWindow(player,playerscr_buttonInfo[player][15].position,&gCamera[player],0,point);
                x=point[0]-25.0f*scale*300.0f/playerscr_buttonZ;
            }
            if(i==1) {
                TB_SetTextColor(gDeleteMenuSelect[player]==1 ? 22 : 1);
                label=gText.indexed[gText.indices[21]+i];
                TB_DrawText(draw_x,y,label);
                label=gPS_PakName[playerscr_gSelectPakIdx[player]];
                TB_DrawText(x,y,label);
            } else if(i==2) {
                delta=65.f*scale*300.0f/playerscr_buttonZ;
                switch(gSelectPlayerNum[player]) {
                case -4:label=gText.text[165];break;
                case -3:label=gText.text[169];break;
                case 0:label=gText.text[148];break;
                case -2:label=gText.text[166];break;
                case -5:label=gText.text[161];break;
                case -1:label=gText.text[168];break;
                }
                draw_x+=delta;
                TB_SetTextColor(0);
                TB_DrawText(TB_CenterText(label,draw_x),y,label);
            } else if(i>=3 && i<11) {
                node=gSelectedPlayer[player];seen=0;
                if(node) {
                    selected=gDeleteMenuSelect[player];
                    do {
                        if((*node)->file && (*(*node)->file)->controller==playerscr_gSelectPakIdx[player]) {
                            if(PROFILE_DISTANCE(i-selected)==seen)break;
                            seen++;
                        }
                        if(i<selected)node=(*node)->previous;else node=(*node)->next;
                    } while(node);
                }
                if(!node) {
                    selected=gDeleteMenuSelect[player];
                    if(selected<i || gSelectPlayerNum[player]<8)node=gPlayerFiles_head;
                    else node=gPlayerFiles_tail;
                    while(node) {
                        if((*node)->file && (*(*node)->file)->controller==playerscr_gSelectPakIdx[player]) {
                            if(PROFILE_DISTANCE(i-((selected<i || gSelectPlayerNum[player]<8) ? 3 : 10))==seen) {seen++;break;}
                            seen++;
                        }
                        if(selected<i || gSelectPlayerNum[player]<8)node=(*node)->next;
                        else node=(*node)->previous;
                    }
                }
                if(node) {
                    label=(*node)->name;
                    if(gNumPlayers==1)limit=98;
                    else limit=(u16)(gNumPlayers==2 ? 70 : 55);
                    if((u32)limit<(u32)HAL_GetTextWidth(label,-1))HAL_SetFontSpacing(0);
                    if(i==gDeleteMenuSelect[player] && node==gSelectedPlayer[player])TB_SetTextColor(22);
                    else {
                        TB_SetTextColor(1);
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
    TB_SetJust(0,3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}
void playerscr_drawSelect(s32);
void drawWait(s32);
void drawCreatePlayer(s32);
void drawDeletePlayer(s32);
s32 playerscr_animText(void *callback_context)
{
    s32 player,x,y;
    HAL_SetTextDepth(0.0f);
    for(player=0;player<gNumPlayers;player++) {
        switch(gPlayerState[player]) {
        case 0:playerscr_drawSelect(player);break;
        case 1:drawWait(player);break;
        case 2:ChangePaneSize(playerscr_pane[player],-20,0,-10,10);break;
        case 3:drawCreatePlayer(player);break;
        case 4:drawDeletePlayer(player);break;
        case 5: {
            u8 text[64];
            y=playerscr_gPlayerRects[gNumPlayers-1][player].y;
            x=playerscr_gPlayerRects[gNumPlayers-1][player].x;
            if(gNumPlayers>=3)HAL_SetFont(11);
            else if(gNumPlayers==2)HAL_SetFont(10);
            else HAL_SetFont(13);
            TB_Sprintf(text,playerscr_pak_full_title_format,gText.text[29],player+1);
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
        case 6: {
            u8 text[64];
            y=playerscr_gPlayerRects[gNumPlayers-1][player].y;
            x=playerscr_gPlayerRects[gNumPlayers-1][player].x;
            if(gNumPlayers>=3)HAL_SetFont(11);
            else if(gNumPlayers==2)HAL_SetFont(10);
            else HAL_SetFont(13);
            TB_Sprintf(text,playerscr_pak_damaged_title_format,gText.text[29],player+1);
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

/* Genuine image-A profile-selection controller. Native entry consumes player in
 * s0 under its actual parent. This kept reconstruction is context, not a match; its genuine parent is absent. */
extern void PlayStartABSound(u32);
extern void PlayLRSound(u32),PlayUDSound(u32);
extern void playerscr_updateSelectInfo(s32),setPlayerState(s32,s32);
extern s32 ReadPlayerData(Profile **);
extern void PS_LockFile(FileInfo **,s8);
extern s8 controlScrActive[];
extern s32 playerscr_buttonsJustInited[],exitPlayerScreen;


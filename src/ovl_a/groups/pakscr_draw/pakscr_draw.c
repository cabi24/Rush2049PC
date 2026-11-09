/* The controller-pak manager's drawing (MAT3 pakscr.c AnimText and its Draw* functions): the paks and their status,
 * a pak's note list, the delete question, and the Rumble Pak and damaged pak notices. */
#include "game/game.h"

/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10). The array
 * extents are the fields' room, not a promise of a terminator (tools/port/capacity.py has the producers' bounds). */
typedef struct Record { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak, fileNo;
    char name[35]; char suffix[15]; u32 blocks; } Record;
VIEW(Record **, gDeleteFile);
#define gDeleteFile gDeleteFile_view

#define TEXT_YES_NO 24              /* gText.tab[]: "yes", "no" */
#define TEXT_DELETE_ACTION 47       /* gText.tab[]: the two lines of the delete combination's instructions */

VIEW(u8, gBlocksNeeded);
#define gBlocksNeeded gBlocksNeeded_view

extern s32 game_sprintf(char *,const char *,...);

/* A pak file record (MAT3 tPS_File) in its PC layout: only the pak it is on. */
typedef struct Owner { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 slot; } Owner;
/* A profile's and a ghost's list node: what they keep of their pak file. */
typedef struct OpenRecord OpenRecord;
struct OpenRecord { OpenRecord **next; void *previous; Owner **owner; };
typedef struct SavedRecord SavedRecord;
struct SavedRecord { SavedRecord **next; Owner **owner; };

/* The note list: the delete combination's instructions (while the selected slot has a note), the columns' heads,
 * the 8 rows shown (number, name with its suffix or "empty", pages; the selected one flashing, its name also
 * under the instructions), and the pak's free pages against the pages a profile needs. */
void DrawFileDel(void)
{
    s32 top=45;
    s32 y;
    s32 row;
    s32 index;
    Record **record;
    s16 x;
    s16 row_y;
    char text[76];
    char suffix[16];
    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    if (gDeleteFile) {
        TB_Strcpy(text,gText.taba[gText.tab[TEXT_DELETE_ACTION]]);
        if (gText.taba[gText.tab[TEXT_DELETE_ACTION]+1]) {
            TB_Strcat(text,pakscr_action_separator);
            TB_Strcat(text,gText.taba[gText.tab[TEXT_DELETE_ACTION]+1]);
        }
        x=TB_CenterText(text,160);
        TB_DrawText(x,top,text);
        top+=10;
    }
    TB_DrawText(35,65,gText.single[84]);
    x=TB_CenterText(gText.single[87],255);
    TB_DrawText(x,65,gText.single[87]);
    y=80;
    HAL_SetFont(14);
    for (row=0;row<gDeleteHeight;row++,y+=12) {
        index=gDeleteSelTop+row;
        if (index>=gDeleteSize) index-=gDeleteSize;
        record=pakscr_FindFile(index);
        TB_SetTextColor(index==gDeleteSelect ? kTB_FlashText : kTB_WhiteText);
        TB_Sprintf(text,pakscr_file_number_format,index+1);
        x=TB_CenterText(text,23);
        row_y=y;
        TB_DrawText(x,row_y,text);
        if (record) {
            TB_Strcpy(text,(*record)->name);
            TB_Strcpy(suffix,(*record)->suffix);
            if (suffix[0]) {
                TB_Strcat(text,pakscr_name_suffix_separator);
                TB_Strcat(text,suffix);
            }
        } else TB_Strcpy(text,gText.single[149]);
        TB_DrawText(35,row_y,text);
        TB_Sprintf(suffix,pakscr_file_blocks_format,record ? (*record)->blocks : 0);
        x=TB_CenterText(suffix,255);
        TB_DrawText(x,row_y,suffix);
        if (index==gDeleteSelect && record) {
            TB_SetTextColor(kTB_WhiteText);
            x=TB_CenterText(text,160);
            TB_DrawText(x,top,text);
            top+=10;
        }
    }
    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    if (gDeleteFile && gText.single[83]) {
        x=TB_CenterText(gText.single[83],160);
        TB_DrawText(x,top,gText.single[83]);
    }
    TB_Sprintf(text,pakscr_free_pages_format,pakscr_gFreePages);
    TB_Strcat(text,pakscr_gFreePages==1 ? gText.single[85] : gText.single[86]);
    TB_Strcat(text,pakscr_gFreePages==1 ? gText.single[91] : gText.single[92]);
    x=TB_CenterText(text,160);
    TB_DrawText(x,182,text);
    game_sprintf(text,pakscr_blocks_needed_format,gText.single[88],gText.single[90],gBlocksNeeded,gText.single[86],gText.single[155]);
    x=TB_CenterText(text,160);
    TB_DrawText(x,192,text);
}

/* Write into str the status of controller `player`'s pak: no controller, no pak, damaged, a Rumble Pak, in use (a
 * profile or a ghost of it is open), room for a profile, or full. */
void GetPakStatusStr(s32 player,char *str)
{
    OpenRecord **open;
    SavedRecord **saved;
    if (!gControllerData[player].attached) TB_Sprintf(str,pakscr_status_no_controller_format,gText.single[165]);
    else if (!PS_PakInserted(player)) TB_Sprintf(str,pakscr_status_no_pak_format,gText.single[168]);
    else if (PS_PakDamaged(player)) TB_Sprintf(str,pakscr_status_damaged_format,gText.single[166]);
    else if (PS_PakRumble(player)) TB_Sprintf(str,pakscr_status_rumble_format,gText.single[169]);
    else {
        for (open=gPlayerFiles.head;open;open=(*open)->next) {
            if ((*open)->owner && (*(*open)->owner)->slot==player) break;
        }
        for (saved=gGhostFiles.head;saved;saved=(*saved)->next) {
            if ((*saved)->owner && (*(*saved)->owner)->slot==player) break;
        }
        if (open || saved) TB_Sprintf(str,pakscr_status_in_use_format,gText.single[159]);
        else if (PS_GetFreeFiles(player) && PS_GetFreeBlocks(player)>=gBlocksNeeded)
            TB_Sprintf(str,pakscr_status_free_format,gText.single[158]);
        else TB_Sprintf(str,pakscr_status_full_format,gText.single[161]);
    }
}

/* Picking a pak: each controller's pak and its status, the chosen one flashing in the larger font. */
void DrawPickPak(void)
{
    char status[24];
    char title[40];
    s32 player;
    s32 y=64;
    s16 x;
    for(player=0;player<4;player++,y+=36) {
        HAL_SetFont(player==gPickedPak ? 10 : 11);
        TB_SetTextColor(player==gPickedPak ? kTB_FlashText : kTB_WhiteText);
        TB_Sprintf(title,pakscr_pak_title_format,gText.single[123],player+1);
        x=TB_CenterText(title,160);
        TB_DrawText(x,y-(player==gPickedPak ? 1 : 0),title);
        GetPakStatusStr(player,status);
        x=TB_CenterText(status,160);
        TB_DrawText(x,y-(player==gPickedPak ? 1 : 0)+12,status);
    }
}


/* "Delete this note?": the question, the note's name, and no / yes, the answer flashing. */
void DrawFileDelVerify(void)
{
    s16 x;
    char title[128];
    char name[128];
    char suffix[16];
    s32 height;
    s32 y=130;
    title[0]=0;
    TB_Strcpy(name,(*gDeleteFile)->name);
    TB_Strcpy(suffix,(*gDeleteFile)->suffix);
    if (suffix[0]) {
        TB_Strcat(name,pakscr_verify_name_separator);
        TB_Strcat(name,suffix);
    }
    TB_Strcpy(title,gText.single[141]);
    TB_Strcat(name,gText.single[142]);
    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    height=TB_GetFontLineHeight();
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    TB_DrawText(160,y-height*3,title);
    HAL_SetFont(14);
    TB_DrawText(160,y-height*2,name);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetFont(pakscr_gYes ? 11 : 10);
    TB_SetTextColor(pakscr_gYes ? kTB_WhiteText : kTB_FlashText);
    x=TB_RightText(gText.taba[gText.tab[TEXT_YES_NO]+1],145);
    TB_DrawText(x,y-(pakscr_gYes ? 0 : 1),gText.taba[gText.tab[TEXT_YES_NO]+1]);
    HAL_SetFont(pakscr_gYes ? 10 : 11);
    TB_SetTextColor(pakscr_gYes ? kTB_FlashText : kTB_WhiteText);
    TB_DrawText(175,y-(pakscr_gYes ? 1 : 0),gText.taba[gText.tab[TEXT_YES_NO]]);
}

/* The screen's text blit callback: the title, then (unless an error box is up) the state's page. Always 1. */
s32 AnimText(void *callback)
{
    s16 x;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    TB_SetTextColor(12);
    x = TB_CenterText(gText.single[157],160);
    TB_DrawText(x,10,gText.single[157]);
    if (gDrawText) {
        switch (gState) {
        case kStatePickPak: DrawPickPak(); break;
        case kStateFileDel: DrawFileDel(); break;
        case kStateFileDelVerify: DrawFileDelVerify(); break;
        case kStateRumblePak: DrawRumblePak(); break;
        case kStateDamaged: DrawDamaged(); break;
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}

/* The Rumble Pak notice: it can't hold notes. */
void DrawRumblePak(void)
{
    char unavailable[256];
    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    TB_Strcpy(unavailable,gText.single[106]);
    TB_Strcat(unavailable,pakscr_rumble_text_separator);
    TB_Strcat(unavailable,gText.single[102]);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    TB_DrawTextScrollBox(160,130,300,160,-1,0,unavailable);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
}

/* The damaged pak notice. */
void DrawDamaged(void)
{
    char failed[256];
    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    TB_Strcpy(failed,gText.single[167]);
    TB_Strcat(failed,pakscr_damaged_text_separator);
    TB_Strcat(failed,gText.single[102]);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    TB_DrawTextScrollBox(160,130,300,160,-1,0,failed);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
}

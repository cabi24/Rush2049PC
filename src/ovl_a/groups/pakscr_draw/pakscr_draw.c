/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: this is MAT3 pakscr.c's draw side. Image A's callback region is laid out by a
 * forward scan of each file in source order with a post-order DFS (last call first) from every procedure not yet
 * emitted (recordscr: drawSelect before drawView's subtree; playerscr: drawSelect .. drawPakDamaged stubs; circuitscr:
 * drawReview first). For pakscr that gives: _GetPakStatusStr (source line 224, reached before _AnimText at 251) =
 * stub 0x803A6F14, then _AnimText's DFS: _DrawDamaged 6F1C, _DrawRumblePak 6F24 (case 4 first), _DrawFileDelVerify
 * 6F2C, _DrawFileDel 7180, _DrawPickPak 7620 (its _GetPakStatusStr already emitted: no stub before it), _AnimText 7918.
 * So _GetPakStatusStr(int pak, char *str) is now the non-kept helper GetPakStatusStr inlined into _DrawPickPak (the
 * status-text chain moved verbatim; the buffer is MAT3's statStr[24]): three of _DrawPickPak's four unused slots were
 * that inlined call's frame area (one remains, disclosed). _AnimText's cases 4/3 are _DrawDamaged / _DrawRumblePak =
 * DrawDamaged / DrawRumblePak (bodies moved verbatim with their 256-byte buffers); its three unused slots are gone.
 * (Our single-root group compile emits _GetPakStatusStr's stub just before _DrawPickPak; the retail slot follows from
 * the source-order scan, which the stand-alone group compile does not reproduce. Stub bytes are identical.) */
/* Image A file-menu renderer: supersede the installed same-name group.
 * New exact member DrawFileDel (1184 bytes); preserve 6F2C/7620/7918
 * (1932 bytes) and claim all four verified members. The real 7918 root
 * retains its genuine private-register renderer context. No arcade
 * ancestry is established.
 *
 * SOLE NEW EXECUTABLE EDIT: text[100] -> text[76] in 7180. Every writer
 * is independently bounded: authentic six-language resources, signed
 * decimal conversions and the native fixed-width Pak-name encoder plus
 * two-encoding copy/append rules. The largest output is 45 bytes with
 * terminator; suffix remains16, bounded at11. This is a safe layout
 * inference, NOT recovery of the original source capacity.
 * Stock M148/T28/P4 -> M124/T28/P4 closes frame192 ->168 while preserving
 * every native buffer address and instruction: suffix52, text68. Source
 * scalar homes are above the 76-byte text object; no overlap is required.
 * Limits: valid loaded resource tables and Pak nodes produced by the two
 * native fixed-width producer paths; arbitrary pointer mutation excluded.
 *
 * INHERITED SHAPING, unchanged: 6F2C two unused s32 slots close312 ->320;
 * 7620 one unused s32 slot (was four) after title[40]; status is MAT3's statStr[24];
 * 7918 a real s16 X carrier (its three unused s32 slots are gone, see below).
 * Other inherited buffer capacities remain unproven and unchanged.
 * No new padding, dummy local, fake signature, synthetic helper, forced
 * register, artificial use, compiler modification or source reordering.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
/* Accessed field offsets are known; the view extents are not string-capacity
 * or termination guarantees. This view preserves existing offsets; the Pak producer bounds are in capacity.py. */
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Record { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak, fileNo;
    char name[35]; char suffix[15]; u32 blocks; } Record;
typedef struct TextIndices { u8 prefix[48]; u16 choice; u8 unknown32[44]; u16 file_action; } TextIndices;
typedef struct Language { void *header; char **labels; void *unknown8; TextIndices *indices; char **choices; } Language;
extern Record **gDeleteFile;
extern Language gText;
extern s8 pakscr_gYes,gDrawText;
extern s32 gState;
extern char pakscr_verify_name_separator[],pakscr_rumble_text_separator[],pakscr_damaged_text_separator[];
extern char *TB_Strcpy(char *,const char *);
extern char *TB_Strcat(char *,const char *);
extern void HAL_SetFont(s32);
extern void TB_SetTextColor(s32);
extern void TB_SetJust(s32,s32);
extern s16 TB_GetFontLineHeight(void);
extern s16 TB_RightText(const char *,s16);
extern s16 TB_CenterText(const char *,s16);
extern void TB_DrawText(s16,s16,const char *);
extern void HAL_SetTextDepth(float);
extern void TB_DrawTextScrollBox(s16,s16,s16,s16,s32,s32,const char *);
extern s16 pakscr_gFreePages,gDeleteSelect,gDeleteSize,gDeleteSelTop,gDeleteHeight,gPickedPak;
extern u8 gBlocksNeeded;
extern char pakscr_action_separator[],pakscr_file_number_format[],pakscr_name_suffix_separator[],pakscr_file_blocks_format[],pakscr_free_pages_format[],pakscr_blocks_needed_format[];
extern char pakscr_pak_title_format[],pakscr_status_no_controller_format[],pakscr_status_no_pak_format[],pakscr_status_damaged_format[],pakscr_status_rumble_format[],pakscr_status_in_use_format[],pakscr_status_free_format[],pakscr_status_full_format[];
extern void TB_Sprintf(char *,const char *,...);
extern s32 game_sprintf(char *,const char *,...);
extern Record **pakscr_FindFile(s32);
typedef struct Slot { s8 present; u8 unknown[15]; } Slot;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Owner { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 slot; } Owner;
typedef struct OpenRecord OpenRecord;
struct OpenRecord { OpenRecord **next; void *previous; Owner **owner; };
typedef struct SavedRecord SavedRecord;
struct SavedRecord { SavedRecord **next; Owner **owner; };
extern Slot gControllerData[];
extern struct PlayerFiles { u32 unknown0, unknown4; OpenRecord **head, **tail; } gPlayerFiles;   /* the profiles on the paks */
/* The ghost files on the paks (carscr.c's RecordList): first the list head. */
typedef struct RecordList {u8 indirect,doubly,unknown02[2];s32 count;SavedRecord **first, **last;} RecordList;
extern RecordList gGhostFiles;
extern s8 PS_PakInserted(s32),PS_PakDamaged(s32),PS_PakRumble(s32);
extern s32 PS_GetFreeFiles(s32);
extern u32 PS_GetFreeBlocks(s32);

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
    TB_SetTextColor(1);
    if (gDeleteFile) {
        TB_Strcpy(text,gText.choices[gText.indices->file_action]);
        if (gText.choices[gText.indices->file_action+1]) {
            TB_Strcat(text,pakscr_action_separator);
            TB_Strcat(text,gText.choices[gText.indices->file_action+1]);
        }
        x=TB_CenterText(text,160);
        TB_DrawText(x,top,text);
        top+=10;
    }
    TB_DrawText(35,65,gText.labels[84]);
    x=TB_CenterText(gText.labels[87],255);
    TB_DrawText(x,65,gText.labels[87]);
    y=80;
    HAL_SetFont(14);
    for (row=0;row<gDeleteHeight;row++,y+=12) {
        index=gDeleteSelTop+row;
        if (index>=gDeleteSize) index-=gDeleteSize;
        record=pakscr_FindFile(index);
        TB_SetTextColor(index==gDeleteSelect ? 22 : 1);
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
        } else TB_Strcpy(text,gText.labels[149]);
        TB_DrawText(35,row_y,text);
        TB_Sprintf(suffix,pakscr_file_blocks_format,record ? (*record)->blocks : 0);
        x=TB_CenterText(suffix,255);
        TB_DrawText(x,row_y,suffix);
        if (index==gDeleteSelect && record) {
            TB_SetTextColor(1);
            x=TB_CenterText(text,160);
            TB_DrawText(x,top,text);
            top+=10;
        }
    }
    HAL_SetFont(11);
    TB_SetTextColor(1);
    if (gDeleteFile && gText.labels[83]) {
        x=TB_CenterText(gText.labels[83],160);
        TB_DrawText(x,top,gText.labels[83]);
    }
    TB_Sprintf(text,pakscr_free_pages_format,pakscr_gFreePages);
    TB_Strcat(text,pakscr_gFreePages==1 ? gText.labels[85] : gText.labels[86]);
    TB_Strcat(text,pakscr_gFreePages==1 ? gText.labels[91] : gText.labels[92]);
    x=TB_CenterText(text,160);
    TB_DrawText(x,182,text);
    game_sprintf(text,pakscr_blocks_needed_format,gText.labels[88],gText.labels[90],gBlocksNeeded,gText.labels[86],gText.labels[155]);
    x=TB_CenterText(text,160);
    TB_DrawText(x,192,text);
}

void GetPakStatusStr(s32,char *);
/* _GetPakStatusStr (MAT3 pakscr.c): inlined into _DrawPickPak DrawPickPak */
void GetPakStatusStr(s32 player,char *str)
{
    OpenRecord **open;
    SavedRecord **saved;
    if (!gControllerData[player].present) TB_Sprintf(str,pakscr_status_no_controller_format,gText.labels[165]);
    else if (!PS_PakInserted(player)) TB_Sprintf(str,pakscr_status_no_pak_format,gText.labels[168]);
    else if (PS_PakDamaged(player)) TB_Sprintf(str,pakscr_status_damaged_format,gText.labels[166]);
    else if (PS_PakRumble(player)) TB_Sprintf(str,pakscr_status_rumble_format,gText.labels[169]);
    else {
        for (open=gPlayerFiles.head;open;open=(*open)->next) {
            if ((*open)->owner && (*(*open)->owner)->slot==player) break;
        }
        for (saved=gGhostFiles.first;saved;saved=(*saved)->next) {
            if ((*saved)->owner && (*(*saved)->owner)->slot==player) break;
        }
        if (open || saved) TB_Sprintf(str,pakscr_status_in_use_format,gText.labels[159]);
        else if (PS_GetFreeFiles(player) && PS_GetFreeBlocks(player)>=gBlocksNeeded)
            TB_Sprintf(str,pakscr_status_free_format,gText.labels[158]);
        else TB_Sprintf(str,pakscr_status_full_format,gText.labels[161]);
    }
}

void DrawPickPak(void)
{
    char status[24];
    char title[40];
    s32 residual_slot1; /* SHAPING (disclosed): one unused 4-byte slot; frame 144 (136 without it) */
    s32 player;
    s32 y=64;
    s16 x;
    for(player=0;player<4;player++,y+=36) {
        HAL_SetFont(player==gPickedPak ? 10 : 11);
        TB_SetTextColor(player==gPickedPak ? 22 : 1);
        TB_Sprintf(title,pakscr_pak_title_format,gText.labels[123],player+1);
        x=TB_CenterText(title,160);
        TB_DrawText(x,y-(player==gPickedPak ? 1 : 0),title);
        GetPakStatusStr(player,status);
        x=TB_CenterText(status,160);
        TB_DrawText(x,y-(player==gPickedPak ? 1 : 0)+12,status);
    }
}


void DrawFileDelVerify(void)
{
    s32 residual_slot0, residual_slot1;
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
    TB_Strcpy(title,gText.labels[141]);
    TB_Strcat(name,gText.labels[142]);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    height=TB_GetFontLineHeight();
    TB_SetJust(1,1);
    TB_DrawText(160,y-height*3,title);
    HAL_SetFont(14);
    TB_DrawText(160,y-height*2,name);
    TB_SetJust(0,3);
    HAL_SetFont(pakscr_gYes ? 11 : 10);
    TB_SetTextColor(pakscr_gYes ? 1 : 22);
    x=TB_RightText(gText.choices[gText.indices->choice+1],145);
    TB_DrawText(x,y-(pakscr_gYes ? 0 : 1),gText.choices[gText.indices->choice+1]);
    HAL_SetFont(pakscr_gYes ? 10 : 11);
    TB_SetTextColor(pakscr_gYes ? 22 : 1);
    TB_DrawText(175,y-(pakscr_gYes ? 1 : 0),gText.choices[gText.indices->choice]);
}

void DrawRumblePak(void);
void DrawDamaged(void);
s32 AnimText(void *callback)
{
    s16 x;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    TB_SetTextColor(12);
    x = TB_CenterText(gText.labels[157],160);
    TB_DrawText(x,10,gText.labels[157]);
    if (gDrawText) {
        switch (gState) {
        case 0: DrawPickPak(); break;
        case 1: DrawFileDel(); break;
        case 2: DrawFileDelVerify(); break;
        case 3: DrawRumblePak(); break;
        case 4: DrawDamaged(); break;
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}

/* _DrawRumblePak (MAT3 pakscr.c): inlined into _AnimText AnimText */
void DrawRumblePak(void)
{
    char unavailable[256];
        HAL_SetFont(11);
        TB_SetTextColor(1);
        TB_Strcpy(unavailable,gText.labels[106]);
        TB_Strcat(unavailable,pakscr_rumble_text_separator);
        TB_Strcat(unavailable,gText.labels[102]);
        TB_SetJust(1,1);
        TB_DrawTextScrollBox(160,130,300,160,-1,0,unavailable);
        TB_SetJust(0,3);
}

/* _DrawDamaged (MAT3 pakscr.c): inlined into _AnimText AnimText */
void DrawDamaged(void)
{
    char failed[256];
        HAL_SetFont(11);
        TB_SetTextColor(1);
        TB_Strcpy(failed,gText.labels[167]);
        TB_Strcat(failed,pakscr_damaged_text_separator);
        TB_Strcat(failed,gText.labels[102]);
        TB_SetJust(1,1);
        TB_DrawTextScrollBox(160,130,300,160,-1,0,failed);
        TB_SetJust(0,3);
}

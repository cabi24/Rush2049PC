/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* pause_TitlePane = _TitlePane@pausescr.c (packaging, mat3_ui_pause_title_20261017,
 * body unchanged; its static header_lock/header_unlock/header_font helpers renamed vs_* and made non-static
 * (not on keep, internal) so their deleted-procedure stubs carry symbols (rule). pause_TitlePane is internal
 * (private IPA: s0-s4 unsaved); real callers func_800B4FB0 = setPauseState (this group) and the unmatched
 * updatePauseScreen = updatePauseScreen, which calls the byte-identical body by jal.
 * B438C pausescr title pane. Donor local types and 64-byte buffer from
 * GC DWARF 0x1689C2; MAT3 pausescr::_TitlePane.
 * N64 lacks the GC null-save fallback and uses single[233], tab[56].
 * N64 computes width/2 at the final call rather than assigning width/=2
 * before TB_GetFontLineHeight. No unused locals or padding arrays are added.
 * Genuine font lock/select/unlock helper nesting; one physical line preserves
 * source-location ties as established for the options title pane.
 */
typedef signed char s8;typedef unsigned char u8;typedef signed short s16;typedef unsigned short u16;typedef signed int s32;
#define F(p,t,o) (*(t *)((u8 *)(p)+(o)))
/* PC: gText (gText) has host pointers (port/assets/text.c), so its header fields and string-table
 * entries sit at host offsets: TXH maps an N64 header offset, TXS an N64 string-table byte offset. */
#define TXH(o) ((o) == 4 ? 8 : (o) == 12 ? 24 : (o) == 16 ? 32 : (o))
#define TXS(o) ((o) / 4 * (int)sizeof(char *))
#define INPUT_SIZE 80   /* an input record (gPlayers): its pointer at +0x48 is 8 bytes (POINTERS.md 10) */
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */
extern u8 text_lock_queue[],pause_titlePos[],gPlayers_save[],gText[];
extern char pause_title_format[],pause_title_name_format[];
extern s32 gPauseState,playerInPause;
extern s8 pause_gControllerRemoved;
extern void *pause_gTitlePane;
s32 osRecvMesg(void *,void *,s32);s32 osSendMesg(void *,void *,s32);
s32 HAL_SetFontInternal(s32);
void TB_Sprintf(char *,const char *,...);
s32 HAL_GetTextWidth(u8 *,s16);
s16 TB_GetFontLineHeight(void);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
void ChangePaneSize(void *,s32,s32,s32,s32);
void vs_header_lock(void) { osRecvMesg(text_lock_queue,0,1); } void vs_header_unlock(void) { osSendMesg(text_lock_queue,0,0); } s32 vs_header_font(s32 font) { s32 old; vs_header_lock(); old=HAL_SetFontInternal(font); vs_header_unlock(); return old; }
void pause_TitlePane(void)
{
    s32 x,y,width;
    unsigned char *str;
    unsigned char text[64];
    vs_header_font(13);
    str = text;
    if(pause_gControllerRemoved) {
        TB_Sprintf(text,pause_title_format,playerInPause+1,F(F(gText,void *,TXH(4)),char *,TXS(932)));
        width=HAL_GetTextWidth(str,-1);
    } else if(gPauseState==7 || gPauseState==8) {
        width=HAL_GetTextWidth(F(gText,u8 **,TXH(16))[F(F(gText,void *,TXH(12)),u16,112)+gPauseState],-1);
    } else if(gPauseState==6) {
        width=0;
    } else {
        TB_Sprintf(text,pause_title_name_format,(u8 *)*F(gPlayers_save+playerInPause*INPUT_SIZE,void **,0)+PROFILE_OFS(20),F(gText,char **,TXH(16))[F(F(gText,void *,TXH(12)),u16,112)]);
        width=HAL_GetTextWidth(str,-1);
    }
    x=F(pause_titlePos,s16,0);
    y=F(pause_titlePos,s16,2);
    if(!pause_gTitlePane) pause_gTitlePane=InitPane(-20,0,-10,10,176,0,0,0);
    if(!width) {
        ChangePaneSize(pause_gTitlePane,-20,0,-10,10);
    } else {
        ChangePaneSize(pause_gTitlePane,x-width/2-8,y-4,width/2+x+8,TB_GetFontLineHeight()+y+4);
    }
}

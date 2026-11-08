/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* controls_updateButtons = updateButtons@controlscr.c (proven), not name entry; source not
 * original-source recovery. Genuine D91A0 caller and D8154 predicate retained.
 * Stock IDO 5.3: strict MATCH, 699 words; frame232 and owned literals agree.
 * Group/private ABI only: caller D91A0 remains nonmatching; no install claim.
 *
 * Disclosed shaping: four unused scalar homes (span, distance, current_y,
 * frame_residual3), 16 bytes total; deleting all four gives frame216 and
 * 20/699 differences. Three are historical carrier slots after their values
 * were restored to repeated expressions. Five compiled-out reads below set
 * constant/index/spacing/count web priorities; no original spelling is asserted.
 * The first-loop count increment follows the conditional before increment.
 * as1 hoists it and leaves a distinct empty target block, preventing the second
 * branch delay-copy. The following no-code count read restores its priority.
 * D8002EB94 and D80140BDC volatile views follow accepted sibling interfaces
 * and native repeated reads, not proof of asynchronous writers. See RESULTS.
 */
typedef signed char s8; typedef unsigned char u8;
typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
typedef struct Item { s32 type, handle; f32 angle, matrix[3][3], position[3]; u8 alpha, pad[3]; } Item;
typedef union Color { u32 word; u8 channel[4]; } Color;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Node { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 color; u8 tail[4]; } Node;
extern Item controls_buttonInfo[4][17];
extern Node ObjInstanceList[];
extern f32 unitmatrix[3][3];
extern volatile f32 gFrameSeconds;
extern f32 controls_gButtonScale,controls_gButtonSpace,controls_buttonX,controls_maxY,controls_minY,controls_buttonZ,controls_cursorZ;
extern s32 controls_buttonsInited[],controls_buttonsJustInited[],controls_updateButtonColor;
/* PC: the button colour controls_updateButtonColor is R,G,B,A bytes in the ROM, which the loader swaps as the word it is declared;
 * PC_RGBA gives back the word whose bytes the alpha store and the scene node's colour expect (docs/port/ENDIAN.md). */
extern f32 controls_cursorSpeed[],controls_scrollSpeed[];
extern s16 gControlSelect[];
extern volatile u8 gNumModels;
extern char controls_selected_texture_name[],controls_unselected_texture_name[];
void ScaleUV(f32[3][3],f32[3][3],f32);
s32 controls_validOption(s8,s32);
void ZOID_SetTexture(s16,intptr_t,s32);
typedef struct NameEntry NameEntry;
NameEntry *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);
void WPitchUV(f32,f32[3][3]);
void WRollUV(f32,f32[3][3]);
void *memcpy(void *,const void *,u32);
void MBOX_HideObject(s32,s32,u32);
void MBOX_ShowObject(s32,s32,u32);
f32 fabsf(f32);
#pragma intrinsic(fabsf)
#define PI 3.1415927f
#define HALF_PI 1.5707964f
#define TWO_PI 6.2831855f
#define FOUR_PI 12.566371f

void controls_updateButtons(s8 player) {
    s8 settled;
    s32 count;
    s32 before;
    s32 i;
    s32 j;
    f32 previous[3];
    f32 span;
    f32 selected_y;
    f32 y;
    f32 distance;
    f32 current_y;
    s16 texture;
    Color color;
    intptr_t tex;
    s32 valid;
    s32 shown;
    s32 mask;
    s16 handle;
    f32 spacing;
    f32 top;
    s32 frame_residual3;

    settled = 1;
    color.word = PC_RGBA(controls_updateButtonColor);
    if (controls_buttonsInited[player] != 0) {
        count = 0;
        before = 0;
        for (j = 0; j != 12; j++) {
            if (j == 10) {}
            if (controls_validOption(player,j)) {
                if (j < gControlSelect[player]) before++;
                count++;
                if (count) {}
            }
        }
        if ((controls_maxY - controls_minY) < (count-1) * controls_gButtonSpace * controls_gButtonScale) {
            selected_y = controls_maxY - (before * (controls_maxY - controls_minY))/(count-1);
        } else {
            selected_y = controls_maxY - before * controls_gButtonSpace * controls_gButtonScale;
        }
        memcpy(previous, controls_buttonInfo[player][14].position, 12);
        controls_buttonInfo[player][14].angle += TWO_PI * gFrameSeconds;
        if (controls_buttonInfo[player][14].angle > TWO_PI) controls_buttonInfo[player][14].angle -= TWO_PI;
        controls_buttonInfo[player][14].position[0] = controls_buttonX * controls_cursorZ / controls_buttonZ + -110.0f * controls_gButtonScale;
        controls_buttonInfo[player][14].position[1] = selected_y * controls_cursorZ / controls_buttonZ;
        controls_buttonInfo[player][14].position[2] = controls_cursorZ;
        if (!controls_buttonsJustInited[player]) {
            if (controls_cursorSpeed[player] < fabsf(controls_buttonInfo[player][14].position[1] - previous[1]) * 60 * controls_gButtonScale / 10) {
                controls_cursorSpeed[player] = fabsf(controls_buttonInfo[player][14].position[1] - previous[1]) * 60.0f * controls_gButtonScale / 10.0f;
                controls_cursorSpeed[player] = controls_cursorSpeed[player] < 540.0f * controls_gButtonScale ? controls_cursorSpeed[player] : 540.0f * controls_gButtonScale;
                controls_cursorSpeed[player] = controls_cursorSpeed[player] > 180.0f * controls_gButtonScale ? controls_cursorSpeed[player] : 180.0f * controls_gButtonScale;
            }
            if (controls_cursorSpeed[player] * gFrameSeconds + previous[1] < controls_buttonInfo[player][14].position[1]) {
                controls_buttonInfo[player][14].position[1] = previous[1] + controls_cursorSpeed[player] * gFrameSeconds;
            } else if (previous[1] - controls_cursorSpeed[player] * gFrameSeconds > controls_buttonInfo[player][14].position[1]) {
                controls_buttonInfo[player][14].position[1] = previous[1] - controls_cursorSpeed[player] * gFrameSeconds;
            } else {
                controls_cursorSpeed[player] = 180.0f * controls_gButtonScale;
            }
        }
        ScaleUV(unitmatrix,controls_buttonInfo[player][14].matrix,controls_gButtonScale * 0.4f);
        WRollUV(controls_buttonInfo[player][14].angle,controls_buttonInfo[player][14].matrix);
        WPitchUV(HALF_PI,controls_buttonInfo[player][14].matrix);
        y = before * controls_gButtonSpace * controls_gButtonScale + selected_y;
        for (i = 0; i != 12; i++) {
            if (i == 10) {}
            if (i == gControlSelect[player]) {
                if (controls_buttonInfo[player][i].angle < PI) controls_buttonInfo[player][i].angle += FOUR_PI * gFrameSeconds;
                if (controls_buttonInfo[player][i].angle > PI) controls_buttonInfo[player][i].angle = PI;
                if (controls_buttonsJustInited[player]) controls_buttonInfo[player][i].angle = PI;
            } else {
                if (controls_buttonInfo[player][i].angle > 0.0f) controls_buttonInfo[player][i].angle -= FOUR_PI * gFrameSeconds;
                if (controls_buttonInfo[player][i].angle < 0.0f) controls_buttonInfo[player][i].angle = 0.0f;
                if (controls_buttonsJustInited[player]) controls_buttonInfo[player][i].angle = 0.0f;
            }
            memcpy(previous, controls_buttonInfo[player][i].position, 12);
            controls_buttonInfo[player][i].position[0] = -40.0f * controls_gButtonScale + controls_buttonX;
            controls_buttonInfo[player][i].position[1] = y;
            controls_buttonInfo[player][i].position[2] = controls_buttonZ;
            if (i == 10) controls_buttonInfo[player][i].position[0] -= 20 * controls_gButtonScale;
            else if (i == 11) controls_buttonInfo[player][i].position[0] += 20.0f * controls_gButtonScale;
            if (!controls_buttonsJustInited[player]) {
                if (controls_scrollSpeed[player] < fabsf(controls_buttonInfo[player][i].position[1] - previous[1]) * 60 / 10) {
                    controls_scrollSpeed[player] = fabsf(controls_buttonInfo[player][i].position[1] - previous[1]) * 60.0f / 10.0f;
                    controls_scrollSpeed[player] = controls_scrollSpeed[player] < 540.0f * controls_gButtonScale ? controls_scrollSpeed[player] : 540.0f * controls_gButtonScale;
                    controls_scrollSpeed[player] = controls_scrollSpeed[player] > 180.0f * controls_gButtonScale ? controls_scrollSpeed[player] : 180.0f * controls_gButtonScale;
                }
                if (controls_scrollSpeed[player] * gFrameSeconds + previous[1] < controls_buttonInfo[player][i].position[1]) {
                    controls_buttonInfo[player][i].position[1] = previous[1] + controls_scrollSpeed[player] * gFrameSeconds;
                    settled = 0;
                } else if (previous[1] - controls_scrollSpeed[player] * gFrameSeconds > controls_buttonInfo[player][i].position[1]) {
                    controls_buttonInfo[player][i].position[1] = previous[1] - controls_scrollSpeed[player] * gFrameSeconds;
                    settled = 0;
                }
            }
            ScaleUV(unitmatrix,controls_buttonInfo[player][i].matrix,controls_gButtonScale);
            WPitchUV(controls_buttonInfo[player][i].angle-HALF_PI,controls_buttonInfo[player][i].matrix);
            if (controls_buttonInfo[player][i].angle > HALF_PI) tex=(intptr_t)MBOX_FindTexture_WORK(controls_selected_texture_name,&texture,0,(s8)(gNumModels-1),1);
            else tex=(intptr_t)MBOX_FindTexture_WORK(controls_unselected_texture_name,&texture,0,(s8)(gNumModels-1),1);
            ZOID_SetTexture((s16)controls_buttonInfo[player][i].handle,(intptr_t)tex,-1);
            if (i == 10) {}
            top = controls_maxY;
            spacing = controls_gButtonSpace*controls_gButtonScale;
            if (spacing) {}
            if (controls_buttonInfo[player][i].position[1] > top + spacing || controls_buttonInfo[player][i].position[1] < controls_minY-spacing) controls_buttonInfo[player][i].alpha=0;
            else if (controls_buttonInfo[player][i].position[1] > controls_maxY) controls_buttonInfo[player][i].alpha=255-(controls_buttonInfo[player][i].position[1]-controls_maxY)*255.0f/(spacing);
            else if (controls_buttonInfo[player][i].position[1] < controls_minY) controls_buttonInfo[player][i].alpha=255-(controls_minY-controls_buttonInfo[player][i].position[1])*255.0f/(spacing);
            else controls_buttonInfo[player][i].alpha=255;
            valid=controls_validOption(player,i);
            if(!valid)controls_buttonInfo[player][i].alpha=0;
            color.channel[3]=controls_buttonInfo[player][i].alpha;
            handle = controls_buttonInfo[player][i].handle;
            ObjInstanceList[handle].color=color.word;
            if (i==10) {
                controls_buttonInfo[player][12].position[0]=controls_buttonInfo[player][i].position[0];
                controls_buttonInfo[player][12].position[1]=controls_buttonInfo[player][i].position[1];
                controls_buttonInfo[player][12].position[2]=controls_buttonInfo[player][i].position[2];
                controls_buttonInfo[player][12].position[0]+=180.0f*controls_gButtonScale;
                ScaleUV(unitmatrix,controls_buttonInfo[player][12].matrix,controls_gButtonScale);
                WPitchUV(controls_buttonInfo[player][i].angle-HALF_PI,controls_buttonInfo[player][12].matrix);
                ZOID_SetTexture((s16)controls_buttonInfo[player][12].handle,(intptr_t)tex,-1);
                handle = controls_buttonInfo[player][12].handle;
                ObjInstanceList[handle].color=color.word;
            }
            shown=controls_validOption(player,i);
            if(!shown) {
                MBOX_HideObject(controls_buttonInfo[player][i].handle,1,15);
                if(i==10)MBOX_HideObject(controls_buttonInfo[player][12].handle,1,15);
            } else if(!controls_buttonInfo[player][i].alpha) {
                MBOX_HideObject(controls_buttonInfo[player][i].handle,1,15);
                if(i==10)MBOX_HideObject(controls_buttonInfo[player][12].handle,1,15);
                y-=controls_gButtonSpace*controls_gButtonScale;
            } else {
                mask=1<<player;
                MBOX_ShowObject(controls_buttonInfo[player][i].handle,0,mask);
                if(i==10)MBOX_ShowObject(controls_buttonInfo[player][12].handle,0,mask);
                y-=controls_gButtonSpace*controls_gButtonScale;
            }
        }
        if(settled==1)controls_scrollSpeed[player]=0.0f;
        controls_buttonsJustInited[player]=0;
    }
}
/* note: identity updateButtons@controlscr.c (MAT3 controlscr.c updateButtons, GC 0x8006D26C). Body is
 * packet unchanged; it now matches inside the group whose caller updateControlScreen also matches.
 * The 'nonmatching caller / no install claim' lines above are historical. */

/* The controls screen's buttons (MAT3 controlscr.c updateButtons): one player's column of control buttons, which
 * scrolls to keep the chosen one in view, turns the chosen button face up and spins the cursor beside it. */
#include "game/game.h"

typedef struct Item { s32 type, handle; f32 angle, matrix[3][3], position[3]; u8 alpha, pad[3]; } Item;
typedef union Color { u32 word; u8 channel[4]; } Color;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
VIEW(Item[4][17], controls_buttonInfo);
#define controls_buttonInfo controls_buttonInfo_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
typedef struct NameEntry NameEntry;

void *memcpy(void *,const void *,u32);

f32 fabsf(f32);
#pragma intrinsic(fabsf)
#define PI 3.1415927f
#define HALF_PI 1.5707964f
#define TWO_PI 6.2831855f
#define FOUR_PI 12.566371f

/* Lays out player's control buttons for this frame: the cursor (button 14) spins beside the chosen control and
 * follows it at a speed that grows with the distance (180 to 540 units a second at scale 1); the buttons stack
 * downward from the chosen one's place, each turning face up (a half turn in a quarter second) when chosen and back
 * when not, textured "selected" past a quarter turn; buttons beyond the column's top and bottom fade out over one
 * button's spacing, and a line that isn't offered is hidden. Button 10 has a twin, 12, beside it. When every button
 * has reached its place the scroll speed starts from 0 again. */
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
    struct tTexture *tex;
    s32 valid;
    s32 shown;
    s32 mask;
    s16 handle;
    f32 spacing;
    f32 top;

    settled = 1;
    color.word = PC_RGBA(controls_updateButtonColor);
    if (controls_buttonsInited[player] != 0) {
        count = 0;
        before = 0;
        for (j = 0; j != 12; j++) {
            if (controls_validOption(player,j)) {
                if (j < gControlSelect[player]) before++;
                count++;
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
            if (controls_buttonInfo[player][i].angle > HALF_PI) tex=MBOX_FindTexture_WORK(controls_selected_texture_name,&texture,0,(s8)(gNumModels[0]-1),1);
            else tex=MBOX_FindTexture_WORK(controls_unselected_texture_name,&texture,0,(s8)(gNumModels[0]-1),1);
            ZOID_SetTexture((s16)controls_buttonInfo[player][i].handle,tex,-1);
            top = controls_maxY;
            spacing = controls_gButtonSpace*controls_gButtonScale;
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
                ZOID_SetTexture((s16)controls_buttonInfo[player][12].handle,tex,-1);
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

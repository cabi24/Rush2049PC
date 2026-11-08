/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned char u8;
typedef signed char s8;
typedef struct Color4 {u8 r,g,b,a;} Color4;
typedef struct Color8 {Color4 color;u8 extra[4];} Color8;
extern s8 gInited_TBText;
extern Color4 gFlashColor2,gFlashColor1;
extern float gColorCycle;
extern Color8 gFontColors[];
extern void TB_TextOncePerFrame(void);
extern void HAL_InstallOncePerFrameFunc(void (*)(void));
extern void HAL_SetTextColor(u8,u8,u8,u8);
void TB_SetTextColor(int mode)
{
 if(mode==22) {
  if(!gInited_TBText) {
   if(!gInited_TBText) {
    gInited_TBText=1;TB_TextOncePerFrame();HAL_InstallOncePerFrameFunc(TB_TextOncePerFrame);
   }
  }
  HAL_SetTextColor(
   (u8)(gFlashColor2.r*(1.0f-gColorCycle)+gFlashColor1.r*gColorCycle),
   (u8)(gFlashColor2.g*(1.0f-gColorCycle)+gFlashColor1.g*gColorCycle),
   (u8)(gFlashColor2.b*(1.0f-gColorCycle)+gFlashColor1.b*gColorCycle),
   (u8)(gFlashColor2.a*(1.0f-gColorCycle)+gFlashColor1.a*gColorCycle));
 } else {
  HAL_SetTextColor(gFontColors[mode].color.r,gFontColors[mode].color.g,
   gFontColors[mode].color.b,gFontColors[mode].color.a);
 }
}

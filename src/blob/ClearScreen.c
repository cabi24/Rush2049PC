/* ClearScreen (MAT3 gfx.c, the N64 form): fills the frame being built with one colour. */
#include "game/game.h"

/* Adds to the frame's display list a fill of the whole screen (screen_width x screen_height) of the frame's colour
 * buffer with color, a 16-bit RGBA5551 colour, between pipe syncs. The colour image is the buffer's pointer, where
 * the N64 gives the RDP its physical address (osVirtualToPhysical; StartGraphics.c says more). */
void ClearScreen(u16 color)
{
    gDPPipeSync(frame_dl++);
    gDPSetColorImage(frame_dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, screen_width, frame_records[frame_record_index].colorBuffer);
    gDPSetFillColor(frame_dl++, ((u32)color << 16) | color);
    gDPFillRectangle(frame_dl++, 0, 0, screen_width - 1, screen_height - 1);
    gDPPipeSync(frame_dl++);
}

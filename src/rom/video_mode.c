/* The video mode (the boot segment's; MAT3's GameCube has its own): the libultra VI mode for the TV type
 * (NTSC, PAL, MPAL), adjusted for the screen's size and the player's offsets (the video screen's centring), applied
 * at a retrace (video_apply_pending_mode, from the scheduler). On the PC port/ultra's VI takes the mode; the
 * picture is the renderer's. */
#include "static_debug_context.h"

/* Makes the next VI mode from the base mode for the screen's size (video_width x video_height) and the offsets: the
 * picture's left, right, top and bottom moved by the offsets and the extra width and height, the scales fitted to
 * them, interlaced modes doubled; the two adjusted modes alternate, and the new one is applied two retraces on
 * (video_pending_frames). With interrupts off. */
void video_build_adjusted_mode(void) {
    u32 saved;
    s32 left, top, right, bottom;
    u32 verticalScale;
    float xRatio, yRatio;
    if (video_base_mode == NULL) return;
    left = video_h_start + video_offset_x;
    right = video_h_end + video_offset_x + video_extra_width;
    top = video_v_start + video_offset_y;
    bottom = video_v_end + video_offset_y + video_extra_height;
    xRatio = (float)(video_h_end - video_h_start) / (float)(right - left);
    yRatio = (float)(video_v_end - video_v_start) / (float)(bottom - top);
    saved = osSetIntMask(1);
    if (video_adjusted_mode == &video_adjusted_modes[0]) {
        video_adjusted_mode = &video_adjusted_modes[1];
    } else {
        video_adjusted_mode = &video_adjusted_modes[0];
    }
    memcpy(video_adjusted_mode, video_base_mode, sizeof(OSViMode));
    left = (s32)((float)left - (1.0f - (float)video_adjusted_mode->comRegs.xScale / (float)video_base_mode->comRegs.xScale) * 5.0f);
    video_adjusted_mode->comRegs.hStart = ((u32)left << 16) | (u32)right;
    video_adjusted_mode->comRegs.xScale = (u32)((float)video_width * xRatio * video_x_scale_factor);
    video_adjusted_mode->fldRegs[0].vStart = ((top & 0x3FF) << 16) | (bottom & 0x3FF);
    video_adjusted_mode->fldRegs[0].origin = video_width * 2;
    verticalScale = (u32)((float)video_height * yRatio * video_y_scale_factor + 16.5f);
    if (video_base_mode->fldRegs[0].origin != video_base_mode->fldRegs[1].origin) {
        video_adjusted_mode->fldRegs[1].origin = video_width * 4;
        video_adjusted_mode->comRegs.width = video_width * 2;
        verticalScale >>= 1;
    } else {
        video_adjusted_mode->fldRegs[1].origin = video_width * 2;
        video_adjusted_mode->comRegs.width = video_width;
    }
    if (video_adjusted_mode->comRegs.ctrl & 0x40) {
        video_adjusted_mode->fldRegs[1].vStart = (((top + 2) & 0x3FF) << 16) | ((bottom + 2) & 0x3FF);
    } else {
        video_adjusted_mode->fldRegs[1].vStart = ((top & 0x3FF) << 16) | (bottom & 0x3FF);
    }
    verticalScale &= 0xFFF0;
    video_adjusted_mode->fldRegs[0].yScale = (video_adjusted_mode->fldRegs[0].yScale & 0xFFFF0000) | verticalScale;
    video_adjusted_mode->fldRegs[1].yScale = (video_adjusted_mode->fldRegs[1].yScale & 0xFFFF0000) | verticalScale;
    video_pending_frames = 2;
    osWritebackDCache(video_adjusted_mode, sizeof(OSViMode));
    osSetIntMask(saved);
}

/* Sets the video mode: tv's retrace rate (PAL 50 Hz, NTSC and MPAL 60) and time, the base mode (libultra's mode
 * table entry mode) and its picture's edges; the first time the TV's default offsets and extra size, later the
 * scene's windows scaled to the new size; then the adjusted mode for width x height. */
void video_set_mode(s32 tv, s32 mode, s32 width, s32 height) {
    if (tv == 0) {
        gRetraceRate = 50.0f;
        gRetraceTime = retrace_time_pal;
    } else {
        gRetraceRate = 60.0f;
        gRetraceTime = retrace_time_ntsc;
    }
    video_tv_type = tv;
    video_mode_index = mode;
    video_base_mode = &osViModeTable[mode];
    video_h_start = (video_base_mode->comRegs.hStart >> 16) & 0x3FF;
    video_h_end = video_base_mode->comRegs.hStart & 0x3FF;
    video_v_start = (video_base_mode->fldRegs[0].vStart >> 16) & 0x3FF;
    video_v_end = video_base_mode->fldRegs[0].vStart & 0x3FF;
    if (video_adjusted_mode == NULL) {
        video_extra_width = video_defaults[video_tv_type].extra_width;
        video_extra_height = video_defaults[video_tv_type].extra_height;
        video_offset_x = video_defaults[video_tv_type].x;
        video_offset_y = video_defaults[video_tv_type].y;
    } else {
        zoid_scale_windows((float)width / (float)video_width, (float)height / (float)video_height);
    }
    video_width = width;
    video_height = height;
    video_build_adjusted_mode();
}

/* Each retrace (from the scheduler): when the pending count runs out, applies the adjusted mode (unblacking the
 * screen if its vertical scale is the base mode's), with gamma, dither filter and divot on. */
void video_apply_pending_mode(void) {
    if (video_pending_frames == 0) return;
    if (--video_pending_frames == 0) {
        if ((video_adjusted_mode->fldRegs[0].yScale & 0xFFFF) == (video_base_mode->fldRegs[0].yScale & 0xFFFF)) {
            osViBlack(0);
        }
        osViSetMode(video_adjusted_mode);
        osViSetSpecialFeatures(0xAA);
    }
}

/* The first index of osViModeTable for the TV type: NTSC 0, PAL 14, MPAL 28. */
s32 video_mode_base_index(void) { s32 offset; if(osTvType==1) offset=0; else if(osTvType==0) offset=14; else offset=28; return offset; }

/* Back to the unadjusted base mode (at a reset). */
void video_restore_base_mode(void) { osViSetMode(video_base_mode); }

/* The TV type's default offsets into *x and *y. */
void video_get_default_offset(register s32 *x, register s32 *y) {
    *x = video_defaults[video_tv_type].x;
    *y = video_defaults[video_tv_type].y;
}

/* The player's offsets from the default into *x and *y. */
void video_get_offset(register s32 *x, register s32 *y) {
    *x = video_offset_x - video_defaults[video_tv_type].x;
    *y = video_offset_y - video_defaults[video_tv_type].y;
}

/* Sets the player's offsets from the default (the video screen's centring), each only if the picture's edges stay
 * within the TV type's bounds (video_offset_bounds), and makes the adjusted mode. */
void video_set_offset(s32 x, s32 y) {
    s32 left, top, right, bottom;
    if (video_adjusted_mode == NULL) return;
    left = video_h_start + x;
    top = video_v_start + y;
    right = video_h_end + x + video_extra_width;
    bottom = video_v_end + y + video_extra_height;
    if (left > video_offset_bounds[video_tv_type][0] && left < video_offset_bounds[video_tv_type][1] && right > video_offset_bounds[video_tv_type][2] && right < video_offset_bounds[video_tv_type][3]) {
        video_offset_x = video_defaults[video_tv_type].x + x;
    }
    if (top > video_offset_bounds[video_tv_type][4] && top < video_offset_bounds[video_tv_type][5] && bottom > video_offset_bounds[video_tv_type][6] && bottom < video_offset_bounds[video_tv_type][7]) {
        video_offset_y = video_defaults[video_tv_type].y + y;
    }
    video_build_adjusted_mode();
}


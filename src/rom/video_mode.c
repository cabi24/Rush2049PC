/* GENERATED ROM-aligned TU — segment 0x21f0 (rom/lib_21f0)
 * layout map fe94ca7f42eb4854cf5b13eed87a4b511f52b70215182a56919a592137bfae27.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "static_debug_context.h"

/* PROMOTED 2026-10-02 — video_build_adjusted_mode
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
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

/* PROMOTED 2026-10-02 — video_set_mode
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
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
        video_extra_width = ((s16 (*)[4])video_default_extra_width)[video_tv_type][0];
        video_extra_height = ((s16 (*)[4])video_default_extra_height)[video_tv_type][0];
        video_offset_x = ((s16 (*)[4])video_default_x)[video_tv_type][0];
        video_offset_y = ((s16 (*)[4])video_default_y)[video_tv_type][0];
    } else {
        zoid_scale_windows((float)width / (float)video_width, (float)height / (float)video_height);
    }
    video_width = width;
    video_height = height;
    video_build_adjusted_mode();
}

/* PROMOTED 2026-10-02 — video_apply_pending_mode
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
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

/* PROMOTED 2026-10-02 — video_mode_base_index
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 video_mode_base_index(void) { s32 offset; if(osTvType==1) offset=0; else if(osTvType==0) offset=14; else offset=28; return offset; }

/* PROMOTED 2026-10-02 — video_restore_base_mode
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void video_restore_base_mode(void) { osViSetMode(video_base_mode); }

/* PROMOTED 2026-10-02 — video_get_default_offset
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void video_get_default_offset(register s32 *x, register s32 *y) {
    *x = ((s16 (*)[4])video_default_x)[video_tv_type][0];
    *y = ((s16 (*)[4])video_default_y)[video_tv_type][0];
}

/* PROMOTED 2026-10-02 — video_get_offset
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void video_get_offset(register s32 *x, register s32 *y) {
    *x = video_offset_x - ((s16 (*)[4])video_default_x)[video_tv_type][0];
    *y = video_offset_y - ((s16 (*)[4])video_default_y)[video_tv_type][0];
}

/* PROMOTED 2026-10-02 — video_set_offset
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void video_set_offset(s32 x, s32 y) {
    s32 left, top, right, bottom;
    if (video_adjusted_mode == NULL) return;
    left = video_h_start + x;
    top = video_v_start + y;
    right = video_h_end + x + video_extra_width;
    bottom = video_v_end + y + video_extra_height;
    if (left > video_offset_bounds[video_tv_type][0] && left < video_offset_bounds[video_tv_type][1] && right > video_offset_bounds[video_tv_type][2] && right < video_offset_bounds[video_tv_type][3]) {
        video_offset_x = ((s16 (*)[4])video_default_x)[video_tv_type][0] + x;
    }
    if (top > video_offset_bounds[video_tv_type][4] && top < video_offset_bounds[video_tv_type][5] && bottom > video_offset_bounds[video_tv_type][6] && bottom < video_offset_bounds[video_tv_type][7]) {
        video_offset_y = ((s16 (*)[4])video_default_y)[video_tv_type][0] + y;
    }
    video_build_adjusted_mode();
}


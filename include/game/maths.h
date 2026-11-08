/* include/game/maths.h: vectors, matrices, angles, random numbers */
#ifndef GAME_MATHS_H
#define GAME_MATHS_H

#include "game/types.h"

/* Data */
extern UnitMatrix unitmatrix;
extern f32 uvector[EXTENT(3)];
extern f32 rvector[EXTENT(3)];
extern u8 blit_ia_palette[EXTENT(4)];
extern u16 blit_tile_lines[EXTENT(258)];
extern CullVertex cull_box_vertices_a[EXTENT(8)];
extern CullVertex cull_box_vertices_b[EXTENT(8)];
extern f32 asin_offsets[EXTENT(4)];
extern f32 invmagnitude_floor;
extern f32 atan_p0;
extern f32 atan_p4;
extern f32 atan_p3;
extern f32 atan_p2;
extern f32 atan_p1;
extern f32 atan_q4;
extern f32 atan_q3;
extern f32 atan_q2;
extern f32 atan_q1;
extern f32 atan_sq2m1;
extern f32 atan_sq2p1;
extern f32 atan_pio2;
extern f32 atan_pio4;
extern const f32 atan2_pio2;
extern const f32 atan2_neg_pio2;
extern const f32 atan2_pi;
extern const f32 atan2_neg_pi;
extern f32 normalvector_floor;
extern f32 upright_min_length;
extern const f32 extract_yaw_epsilon;
extern f32 node_eye_distance;
extern f32 zoid_view_matrix[EXTENT(4)][4];
extern f32 zoid_upright_matrix[EXTENT(4)][4];
extern f32 zoid_node_position[EXTENT(3)];
extern s8 frame_lookat_count;
extern s16 zoid_late_pass_a;
extern s16 zoid_late_node_a;
extern s16 zoid_late_pass_b;
extern s16 zoid_late_node_b;
extern f32 zoid_scaled_matrix[EXTENT(4)][4];
extern CullVertex cull_vertices[EXTENT(27)];
extern volatile s8 frame_record_index;
extern f32 zoid_lookat_matrix[EXTENT(16)];
extern Gfx *zoid_branch_slot;
extern union Vtx *zoid_poly_vertices;
extern CullVertex zoid_poly_vertex_buffers[EXTENT(1)][3200];
extern s32 zoid_current_tlut;
extern struct Texture *zoid_current_texture;
extern s32 zoid_tlut_mode;
#endif /* GAME_MATHS_H */

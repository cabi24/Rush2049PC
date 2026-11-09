/* include/game/maths.h: vectors, matrices, angles, random numbers */
#ifndef GAME_MATHS_H
#define GAME_MATHS_H

#include "game/types.h"

/* Data */
extern UnitMatrix unitmatrix;
extern f32 uvector[EXTENT(3)];
extern f32 rvector[EXTENT(3)];
extern s32 rand_seed;
extern void *video_gTitlePane;
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

/* Functions */
void AnimatePathTarget(struct CamNode *node, s16 flag);
void BodyVector(f32 *src, f32 *dst, f32 *uvs);
void build_far_node_matrix(s32 idx, struct View *obj, u32 *m, f32 scale, s32 absolute);
s32 build_node_matrix(s32 view, struct XForm *src, u32 (*out)[16], f32 w, s32 depth);
s32 build_node_matrix_rows4(s32 view, f32 (*m)[4], f32 *pos, u32 (*out)[16], f32 w, s32 depth);
void CopyUV(f32 (*src)[3], f32 (*dst)[3]);
void CreateQuatMat(f32 *mat, f32 *q, s32 normal);
void CreateYPRMatrix(f32 (*matrix)[3], f32 *pyr);
f32 Distance(f32 x, f32 y, f32 z);
f32 Distance2D(f32 x, f32 z);
f32 dot_uv_y_axis(f32 *a, f32 *b);
void ExtractYaw(f32 *mat, f32 *yaw);
f32 facos(f32 input);
f32 farcsine(f32 x, s32 flag);
f32 fasin(f32 input);
f32 fatan(f32 x);
f32 fatan2(f32 y, f32 x);
void fpitch(f32 sint, f32 cost, f32 (*m)[3]);
void froll(f32 sint, f32 cost, f32 (*m)[3]);
f32 fsatan(f32 x);
f32 fxatan(f32 x);
void fyaw(f32 sint, f32 cost, f32 (*m)[3]);
s32 game_rand(void);
void InterpQuats(f32 frac, f32 *q1, f32 *q2, f32 *qr);
f32 invmagnitude(f32 *vec);
f32 invsqr(f32 val);
void lookat_reflect_fill(struct LookAt *l, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
void mtx_rotation_to_fixed(f32 (*matrix)[4], u32 *fixed);
void MulUVS(f32 (*m)[3], f32 (*in)[3], f32 (*out)[3]);
f32 NormalVector(f32 *v);
void PitchUV(f32 angle, f32 (*uv)[3]);
f32 Random(f32 max);
void ResetPath(struct Camera *cam, s32 idx);
void RollUV(f32 angle, f32 (*uv)[3]);
void RotateUV(f32 *pyr, f32 (*uvs)[3]);
void ScaleUV(f32 (*src)[3], f32 (*dst)[3], f32 scale);
void ScaleUVXYZ(f32 *src, f32 *dst, f32 sx, f32 sy, f32 sz);
f32 TB_Vector2Normalize(f32 *v);
void TransposeUV(f32 *uv);
void U64WorldVector(f32 *bodvec, f32 *uvec, f32 *uvs);
void vector_direction(f32 *vec, f32 *dir);
f32 vector_magnitude(f32 *vec);
void video_TitlePane(void);
void video_updateButtons(void);
s32 video_validOption(s32 i);
void WorldVector(f32 *src, f32 *dst, f32 *uvs);
void WPitchUV(f32 angle, f32 (*uv)[3]);
void WRollUV(f32 angle, f32 (*uv)[3]);
void WYawUV(f32 angle, f32 (*uv)[3]);
void YawUV(f32 angle, f32 (*uv)[3]);
void zoid_build_upright_matrix(f32 *m);
s32 zoid_node_visible(void *arg0, s32 arg1);
s32 zoid_subtree_visible(struct TrackNode *node, s32 type);

#endif /* GAME_MATHS_H */

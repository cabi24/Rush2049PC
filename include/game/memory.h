/* include/game/memory.h: the heap, the asset wads and waves, lists and trees */
#ifndef GAME_MEMORY_H
#define GAME_MEMORY_H

#include "game/types.h"

/* Data */
extern u32 gWaveSizes[EXTENT(183)];
extern u8 gLargestTextWave;
extern f32 gDefStartTimes[EXTENT(28)];
extern OSMesgQueue rom_dma_queue;
extern OSMesgQueue bg_load_idle_queue;
extern s32 D_80110240;
extern s32 D_80110278;
extern s32 D_8011027C;
extern s32 D_80110280;
extern u8 save_gInited;
extern u32 skid_tire_flags[EXTENT(4)];
extern s32 wave_rom_offsets[EXTENT(183)];
extern DeflateConfig D_8011E838[EXTENT(10)];
extern s32 D_8011E888[EXTENT(29)];
extern s32 D_8011E8FC[EXTENT(30)];
extern s32 D_8011E974[EXTENT(19)];
extern u8 D_8011E9C0[EXTENT(19)];
extern static_tree_desc_s D_8011E9D4;
extern static_tree_desc_s D_8011E9E8;
extern static_tree_desc_s D_8011E9FC;
extern char player_name_format[EXTENT(12)];
extern s8 wave_is_model[EXTENT(183)];
extern s32 wave_storage_kinds[EXTENT(183)];
extern f32 skid_lift;
extern ct_data D_801249F0[EXTENT(288)];
extern ct_data D_80124E70[EXTENT(30)];
extern tCarModel car_palettes_palBase[EXTENT(51)];
extern s8 gMirrorWaves;
extern OSMesg audio_cmd_queue_msgs[EXTENT(1)];
extern struct Source *active_emitters_head;
extern OSContStatus cont_status[EXTENT(4)];
extern u8 D_80152260[EXTENT(512)];
extern u8 D_80152468[EXTENT(256)];
extern s32 D_80152578[EXTENT(29)];
extern s32 D_80152600[EXTENT(30)];
extern OSMesgQueue heap_lock_queue;
extern s32 heap_lock_msg;
extern struct Heap *gMainHeap;
extern s32 bg_load_request;
extern OSMesgQueueWords bg_load_request_queue;
extern s32 bg_load_request_messages;
extern anon_loaded_ready_14 D_80156D38[EXTENT(64)];
extern anon_loaded_ready_14 gWaveInstance[EXTENT(64)];
extern u16 gTexList_skid;
/* pc_extra_blbits: declared by its files (src/blob/groups/zlib_deflate/group.c: defines it) */
#endif /* GAME_MEMORY_H */

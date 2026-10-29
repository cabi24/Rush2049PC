/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {u8 first,second;} Flags;
#ifdef TARGET_PC
/* On the PC the finish cameras' state is pc_finish_cam, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md
 * 6): the first two cars' here. */
#define FINISH_CAM_FIRST PC_FINISH_CAM(0)
#define FINISH_CAM_SECOND PC_FINISH_CAM(1)
#else
extern Flags D_80110680;
#define FINISH_CAM_FIRST D_80110680.first
#define FINISH_CAM_SECOND D_80110680.second
#endif
void audio_dsp_process(void);
void mode_flags_clear(void) {FINISH_CAM_FIRST=0;FINISH_CAM_SECOND=0;audio_dsp_process();}

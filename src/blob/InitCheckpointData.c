/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {u8 first,second;} Flags;
/* On the PC the finish cameras' state is pc_finish_cam, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md
 * 6); this clears the first two slots', as on the N64. */
#define FINISH_CAM_FIRST PC_FINISH_CAM(0)
#define FINISH_CAM_SECOND PC_FINISH_CAM(1)
void ReadCheckpointData(void);
void InitCheckpointData(void) {FINISH_CAM_FIRST=0;FINISH_CAM_SECOND=0;ReadCheckpointData();}
